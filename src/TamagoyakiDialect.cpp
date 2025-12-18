//===- TamagoyakiDialect.cpp - Tamagoyaki dialect ---------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamagoyakiDialect.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/TypeSwitch.h"
#include "mlir/IR/IRMapping.h"


using namespace mlir;
using namespace mlir::tama;

#include "TamagoyakiDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Tama dialect.
//===----------------------------------------------------------------------===//

void TamaDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "TamagoyakiOps.cpp.inc"

      >();
  registerTypes();
}

//===----------------------------------------------------------------------===//
// Tama ops
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "TamagoyakiOps.cpp.inc"

LogicalResult EqOp::verify() {
    if (getInputs().empty()) {
        return emitOpError("must have at least one operand");
    }

    for (Value operand : getInputs()) {
        Operation *defOp = operand.getDefiningOp();
        if (defOp && isa<EqOp>(defOp)) {
            return emitOpError("result of an eq operation cannot be used as an operand of another eq");
        }

        for (Operation *user : operand.getUsers()) {
            if (user != getOperation()) {
                return emitOpError("operands must only be used by the eq operation");
            }
        }
    }

    return success();
}

//===----------------------------------------------------------------------===//
// Tamagoyaki passes
//===----------------------------------------------------------------------===//

namespace mlir::tama {
#define GEN_PASS_DEF_TAMAINSERTEGRAPH
#define GEN_PASS_DEF_TAMASWITCHBARFOO
#include "TamagoyakiPasses.h.inc"


namespace {

/// Recursively wraps all values (block arguments and operation results) in EqOps.
/// For each value, creates an EqOp and replaces all uses of the original value
/// with the EqOp's result.
void wrapValuesInEqOps(Region &region, OpBuilder &builder) {
    for (Block &block : region) {
        // Wrap block arguments at the start of the block
        if (!block.getArguments().empty()) {
            builder.setInsertionPointToStart(&block);
            for (BlockArgument arg : block.getArguments()) {
                auto eqOp = EqOp::create(builder, arg.getLoc(), 
                                            TypeRange{arg.getType()}, 
                                            ValueRange{arg});
                arg.replaceAllUsesExcept(eqOp.getResult(), eqOp);
            }
        }
        
        // Collect operations to avoid iterator invalidation when inserting
        SmallVector<Operation*> ops;
        for (Operation &op : block) {
            ops.push_back(&op);
        }
        
        for (Operation *op : ops) {
            // Skip EqOp to avoid wrapping EqOp results (which would violate verification)
            if (isa<EqOp>(op))
                continue;
            
            // Recursively process nested regions first
            for (Region &nestedRegion : op->getRegions()) {
                wrapValuesInEqOps(nestedRegion, builder);
            }
            
            // Wrap each operation result in an EqOp
            if (op->getNumResults() > 0) {
                builder.setInsertionPointAfter(op);
                for (OpResult result : op->getResults()) {
                    auto eqOp = EqOp::create(builder, result.getLoc(), 
                                                TypeRange{result.getType()}, 
                                                ValueRange{result});
                    result.replaceAllUsesExcept(eqOp.getResult(), eqOp);
                }
            }
        }
    }
}

    
class TamaInsertEgraph
    : public impl::TamaInsertEgraphBase<TamaInsertEgraph> {
public:
    using impl::TamaInsertEgraphBase<
        TamaInsertEgraph>::TamaInsertEgraphBase;
    void runOnOperation() final {
        ModuleOp module = getOperation();
        
        // Collect all functions first, then process them
        SmallVector<func::FuncOp> functions;
        for (Operation &op : module.getBody()->getOperations()) {
            if (auto funcOp = dyn_cast<func::FuncOp>(op)) {
                functions.push_back(funcOp);
            }
        }
        
        // Process each function
        for (func::FuncOp funcOp : functions) {
            if (failed(transformFunction(funcOp, true))) {
                signalPassFailure();
            }
        }
    }

private:
    LogicalResult transformFunction(func::FuncOp funcOp, bool insertSingleElementEqs) {
        // Only transform single-block functions
        Region &funcBody = funcOp.getFunctionBody();

        if (!funcBody.hasOneBlock()) {
            return failure();
        }

        Block &entryBlock = funcBody.front();
        auto returnOp = dyn_cast<func::ReturnOp>(*entryBlock.getTerminator());
                
        if (!returnOp) {
            return funcOp.emitOpError("function must have a return operation");
        }

        Location loc = funcOp.getLoc();
        
        // Create the egraphOp at the start of the block
        FunctionType funcType = funcOp.getFunctionType();
        OpBuilder builder(funcOp->getContext());
        auto egraphOp = EGraphOp::create(builder, loc, funcType.getResults(), {});

        // Put the single-block function body in the egraphOp
        Region &egraphBody = egraphOp.getBody();
        egraphBody.takeBody(funcBody);
        
        // Rewrite the func.return to a tama.yield
        builder.setInsertionPoint(returnOp);
        YieldOp::create(builder, returnOp.getLoc(), returnOp.getOperands());
        returnOp.erase();
        
        if (insertSingleElementEqs) {
            wrapValuesInEqOps(egraphBody, builder);
        }
        
        // Create a new function body
        Block *newEntryBlock = builder.createBlock(
            &funcBody, 
            funcBody.end(), 
            funcType.getInputs(),
            SmallVector<Location>(funcType.getNumInputs(), loc)
        );
        
        egraphOp->setOperands(newEntryBlock->getArguments());
    
        // Insert the egraphOp into the new block
        builder.setInsertionPointToStart(newEntryBlock);
        builder.insert(egraphOp);
    
        // Create a return that returns the egraphOp's results
        func::ReturnOp::create(builder, loc, egraphOp->getResults());
        
        return success();
    }
};
    
class TamaSwitchBarFooRewriter : public OpRewritePattern<func::FuncOp> {
public:
  using OpRewritePattern<func::FuncOp>::OpRewritePattern;
  LogicalResult matchAndRewrite(func::FuncOp op,
                                PatternRewriter &rewriter) const final {
    if (op.getSymName() == "bar") {
      rewriter.modifyOpInPlace(op, [&op]() { op.setSymName("foo"); });
      return success();
    }
    return failure();
  }
};

class TamaSwitchBarFoo
    : public impl::TamaSwitchBarFooBase<TamaSwitchBarFoo> {
public:
  using impl::TamaSwitchBarFooBase<
      TamaSwitchBarFoo>::TamaSwitchBarFooBase;
  void runOnOperation() final {
    RewritePatternSet patterns(&getContext());
    patterns.add<TamaSwitchBarFooRewriter>(&getContext());
    FrozenRewritePatternSet patternSet(std::move(patterns));
    if (failed(applyPatternsGreedily(getOperation(), patternSet)))
      signalPassFailure();
  }
};
} // namespace
} // namespace mlir::tama

//===----------------------------------------------------------------------===//
// Tama types
//===----------------------------------------------------------------------===//

#define GET_TYPEDEF_CLASSES
#include "TamagoyakiTypes.cpp.inc"

void TamaDialect::registerTypes() {
  addTypes<
#define GET_TYPEDEF_LIST
#include "TamagoyakiTypes.cpp.inc"

      >();
}
