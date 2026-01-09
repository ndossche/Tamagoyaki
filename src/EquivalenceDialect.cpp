//===- EquivalenceDialect.cpp - Equivalence dialect ---------------*- C++
//-*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EquivalenceDialect.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace mlir::equivalence;

#include "EquivalenceDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Equivalence dialect.
//===----------------------------------------------------------------------===//

void EquivalenceDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "EquivalenceOps.cpp.inc"

      >();
  registerTypes();
}

//===----------------------------------------------------------------------===//
// Equivalence ops
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "EquivalenceOps.cpp.inc"

LogicalResult ClassOp::verify() {
  if (getInputs().empty()) {
    return emitOpError("must have at least one operand");
  }

  for (Value operand : getInputs()) {
    Operation *defOp = operand.getDefiningOp();
    if (defOp && isa<ClassOp>(defOp)) {
      return emitOpError("result of a class operation cannot be used as an "
                         "operand of another class");
    }

    for (Operation *user : operand.getUsers()) {
      if (user != getOperation()) {
        return emitOpError("operands must only be used by the class operation");
      }
    }
  }

  return success();
}

//===----------------------------------------------------------------------===//
// Equivalence passes
//===----------------------------------------------------------------------===//

namespace mlir::equivalence {
#define GEN_PASS_DEF_EQUIVALENCEINSERTGRAPH
#define GEN_PASS_DEF_EQUIVALENCESWITCHBARFOO
#include "EquivalencePasses.h.inc"

namespace {

/// Recursively wraps all values (block arguments and operation results) in
/// ClassOps. For each value, creates a ClassOp and replaces all uses of the
/// original value with the ClassOp's result.
void wrapValuesInClassOps(Region &region, OpBuilder &builder) {
  for (Block &block : region) {
    // Wrap block arguments at the start of the block
    if (!block.getArguments().empty()) {
      builder.setInsertionPointToStart(&block);
      for (BlockArgument arg : block.getArguments()) {
        auto classOp = ClassOp::create(
            builder, arg.getLoc(), TypeRange{arg.getType()}, ValueRange{arg});
        arg.replaceAllUsesExcept(classOp.getResult(), classOp);
      }
    }

    // Collect operations to avoid iterator invalidation when inserting
    SmallVector<Operation *> ops;
    for (Operation &op : block) {
      ops.push_back(&op);
    }

    for (Operation *op : ops) {
      // Skip ClassOp to avoid wrapping ClassOp results (which would violate
      // verification)
      if (isa<ClassOp>(op))
        continue;

      // Recursively process nested regions first
      for (Region &nestedRegion : op->getRegions()) {
        wrapValuesInClassOps(nestedRegion, builder);
      }

      // Wrap each operation result in a ClassOp
      if (op->getNumResults() > 0) {
        builder.setInsertionPointAfter(op);
        for (OpResult result : op->getResults()) {
          auto classOp =
              ClassOp::create(builder, result.getLoc(),
                              TypeRange{result.getType()}, ValueRange{result});
          result.replaceAllUsesExcept(classOp.getResult(), classOp);
        }
      }
    }
  }
}

class EquivalenceInsertGraph
    : public impl::EquivalenceInsertGraphBase<EquivalenceInsertGraph> {
public:
  using impl::EquivalenceInsertGraphBase<
      EquivalenceInsertGraph>::EquivalenceInsertGraphBase;
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
  LogicalResult transformFunction(func::FuncOp funcOp,
                                  bool insertSingleElementEqs) {
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

    // Create the graphOp at the start of the block
    FunctionType funcType = funcOp.getFunctionType();
    OpBuilder builder(funcOp->getContext());
    auto graphOp = GraphOp::create(builder, loc, funcType.getResults(), {});

    // Put the single-block function body in the graphOp
    Region &graphBody = graphOp.getBody();
    graphBody.takeBody(funcBody);

    // Rewrite the func.return to a tama.yield
    builder.setInsertionPoint(returnOp);
    YieldOp::create(builder, returnOp.getLoc(), returnOp.getOperands());
    returnOp.erase();

    if (insertSingleElementEqs) {
      wrapValuesInClassOps(graphBody, builder);
    }

    // Create a new function body
    Block *newEntryBlock = builder.createBlock(
        &funcBody, funcBody.end(), funcType.getInputs(),
        SmallVector<Location>(funcType.getNumInputs(), loc));

    graphOp->setOperands(newEntryBlock->getArguments());

    // Insert the graphOp into the new block
    builder.setInsertionPointToStart(newEntryBlock);
    builder.insert(graphOp);

    // Create a return that returns the graphOp's results
    func::ReturnOp::create(builder, loc, graphOp->getResults());

    return success();
  }
};

class EquivalenceSwitchBarFooRewriter : public OpRewritePattern<func::FuncOp> {
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

class EquivalenceSwitchBarFoo
    : public impl::EquivalenceSwitchBarFooBase<EquivalenceSwitchBarFoo> {
public:
  using impl::EquivalenceSwitchBarFooBase<
      EquivalenceSwitchBarFoo>::EquivalenceSwitchBarFooBase;
  void runOnOperation() final {
    RewritePatternSet patterns(&getContext());
    patterns.add<EquivalenceSwitchBarFooRewriter>(&getContext());
    FrozenRewritePatternSet patternSet(std::move(patterns));
    if (failed(applyPatternsGreedily(getOperation(), patternSet)))
      signalPassFailure();
  }
};
} // namespace
} // namespace mlir::equivalence

//===----------------------------------------------------------------------===//
// Equivalence types
//===----------------------------------------------------------------------===//

#define GET_TYPEDEF_CLASSES
#include "EquivalenceTypes.cpp.inc"

void EquivalenceDialect::registerTypes() {
  addTypes<
#define GET_TYPEDEF_LIST
#include "EquivalenceTypes.cpp.inc"

      >();
}
