//===- PotatoDialect.cpp - Potato dialect ---------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PotatoDialect.h"

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
using namespace mlir::potato;

#include "PotatoDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Potato dialect.
//===----------------------------------------------------------------------===//

void PotatoDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "PotatoOps.cpp.inc"

      >();
  registerTypes();
}

//===----------------------------------------------------------------------===//
// Potato ops
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "PotatoOps.cpp.inc"

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
// Potato passes
//===----------------------------------------------------------------------===//

namespace mlir::potato {
#define GEN_PASS_DEF_POTATOINSERTEGRAPH
#define GEN_PASS_DEF_POTATOSWITCHBARFOO
#include "PotatoPasses.h.inc"


namespace {
    
class PotatoInsertEgraph
    : public impl::PotatoInsertEgraphBase<PotatoInsertEgraph> {
public:
    using impl::PotatoInsertEgraphBase<
        PotatoInsertEgraph>::PotatoInsertEgraphBase;
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
            if (failed(transformFunction(funcOp))) {
                signalPassFailure();
            }
        }
    }

private:
    LogicalResult transformFunction(func::FuncOp funcOp) {
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
        auto egraphOp = EGraphOp::create(builder, loc, funcType.getResults());

        // Put the single-block function body in the egraphOp
        Region &egraphBody = egraphOp.getBody();
        egraphBody.takeBody(funcBody);
        
        // Rewrite the func.return to a potato.yield
        builder.setInsertionPoint(returnOp);
        YieldOp::create(builder, returnOp.getLoc(), returnOp.getOperands());
        returnOp.erase();
        
        // Create a new function body
        Block *newEntryBlock = builder.createBlock(
            &funcBody, 
            funcBody.end(), 
            funcType.getInputs(),
            SmallVector<Location>(funcType.getNumInputs(), loc)
        );
    
        // Insert the egraphOp into the new block
        builder.setInsertionPointToStart(newEntryBlock);
        builder.insert(egraphOp);
    
        // Create a return that returns the egraphOp's results
        func::ReturnOp::create(builder, loc, egraphOp->getResults());
    
        // Remap the block arguments in the egraph body: the old block args
        // now need to reference the new entry block's arguments
        Block &egraphEntryBlock = egraphBody.front();
        for (auto [oldArg, newArg] : llvm::zip(
                 egraphEntryBlock.getArguments(), 
                 newEntryBlock->getArguments())) {
            oldArg.replaceAllUsesWith(newArg);
        }
        
        // Erase the old block arguments from the egraph's block
        egraphEntryBlock.eraseArguments(0, egraphEntryBlock.getNumArguments());
    
        return success();
    }
};
    
class PotatoSwitchBarFooRewriter : public OpRewritePattern<func::FuncOp> {
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

class PotatoSwitchBarFoo
    : public impl::PotatoSwitchBarFooBase<PotatoSwitchBarFoo> {
public:
  using impl::PotatoSwitchBarFooBase<
      PotatoSwitchBarFoo>::PotatoSwitchBarFooBase;
  void runOnOperation() final {
    RewritePatternSet patterns(&getContext());
    patterns.add<PotatoSwitchBarFooRewriter>(&getContext());
    FrozenRewritePatternSet patternSet(std::move(patterns));
    if (failed(applyPatternsGreedily(getOperation(), patternSet)))
      signalPassFailure();
  }
};
} // namespace
} // namespace mlir::potato

//===----------------------------------------------------------------------===//
// Potato types
//===----------------------------------------------------------------------===//

#define GET_TYPEDEF_CLASSES
#include "PotatoTypes.cpp.inc"

void PotatoDialect::registerTypes() {
  addTypes<
#define GET_TYPEDEF_LIST
#include "PotatoTypes.cpp.inc"

      >();
}
