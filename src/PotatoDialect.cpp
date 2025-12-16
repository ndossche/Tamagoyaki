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

namespace mlir::potato {
#define GEN_PASS_DEF_POTATOSWITCHBARFOO
#include "PotatoPasses.h.inc"

//===----------------------------------------------------------------------===//
// Potato passes
//===----------------------------------------------------------------------===//

namespace {
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
