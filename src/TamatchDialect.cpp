//===- TamatchDialect.cpp - Tamatch dialect -----*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamatchDialect.h"

#include "TamagoyakiDialect.h"
#include "mlir/Dialect/PDLInterp/IR/PDLInterp.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>
#include <iostream>
#include <ostream>
#include <utility>

using namespace mlir;
using namespace mlir::tamatch;

#include "TamatchDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Tamatch dialect.
//===----------------------------------------------------------------------===//

void TamatchDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "TamatchOps.cpp.inc"

      >();
}

Type TamatchDialect::parseType(DialectAsmParser &parser) const {
  StringRef typeName;
  if (parser.parseKeyword(&typeName))
    return Type();
  return {};
}

void TamatchDialect::printType(Type type, DialectAsmPrinter &os) const {
  os << "unknown";
}

//===----------------------------------------------------------------------===//
// Tamatch ops
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "TamatchOps.cpp.inc"

//===----------------------------------------------------------------------===//
// Tamatch Passes
//===----------------------------------------------------------------------===//

namespace mlir::tamatch {

// Custom creator function for PDL patterns
static SmallVector<Value> getEqVals(PatternRewriter &rewriter, Value val) {
  if (auto eqOp = dyn_cast<tama::EqOp>(val.getDefiningOp())) {
    return llvm::to_vector(eqOp->getOperands());
  }
  return {val};
}

static Value getEqResult(PatternRewriter &rewriter, Value val) {
  if (auto eqOp =
          val.hasOneUse() ? dyn_cast<tama::EqOp>(*val.user_begin()) : nullptr) {
    return eqOp.getResult();
  }
  return val;
}

static tama::EqOp getEqOp(PatternRewriter &rewriter, Value val) {
  if (auto eqOp =
          val.hasOneUse() ? dyn_cast<tama::EqOp>(*val.user_begin()) : nullptr) {
    return eqOp;
  }

  // If the value is not part of an eclass yet, create one
  OpBuilder builder(val.getContext());
  builder.setInsertionPointAfterValue(val);
  auto eqOp = tama::EqOp::create(builder, val.getLoc(),
                                 TypeRange{val.getType()}, ValueRange{val});
  rewriter.replaceUsesWithIf(
      val, eqOp.getResult(),
      [&eqOp](OpOperand &operand) { return operand.getOwner() != eqOp; });
  return eqOp;
}

class EqOpUnionFind {
public:
  /// Union two individual values
  void eqUnion(PatternRewriter &rewriter, Value a, Value b) {
    tama::EqOp eqA = getEqOp(rewriter, a);
    tama::EqOp eqB = getEqOp(rewriter, b);

    if (isEquivalent(eqA, eqB))
      return;
    // TODO: unionSets always treats the first argument as leader
    // this might lead to an unbalanced union-find?

    tama::EqOp leader = *unionFind.unionSets(eqA, eqB);
    tama::EqOp other = eqB;

    rewriter.replaceAllUsesWith(other.getResult(), leader.getResult());

    // Find operands in `other` that aren't already in `leader`.
    // Operands need to be deduplicated because it can happen that the same
    // operand was used by different parent eclasses after their children were
    // merged
    SmallPtrSet<Value, 8> existing(leader->operand_begin(),
                                   leader->operand_end());
    SmallVector<Value, 8> newOperands;
    for (Value operand : other->getOperands()) {
      if (existing.insert(operand).second)
        newOperands.push_back(operand);
    }
    // add newOperands to the end of the operand list
    leader->setOperands(leader->getNumOperands(), 0, newOperands);

    unionFind.erase(other);
    rewriter.eraseOp(other);
  }

  /// Union an operation's results with corresponding values
  void eqUnion(PatternRewriter &rewriter, Operation *op, ValueRange vals) {
    assert(op->getNumResults() == vals.size() &&
           "Operation result count must match value range size");
    for (auto [result, val] : llvm::zip(op->getResults(), vals))
      eqUnion(rewriter, result, val);
  }

  /// Union two value ranges pairwise
  void eqUnion(PatternRewriter &rewriter, ValueRange a, ValueRange b) {
    assert(a.size() == b.size() && "Value ranges must have equal size");
    for (auto [va, vb] : llvm::zip(a, b))
      eqUnion(rewriter, va, vb);
  }

  /// Check if two values are in the same equivalence class
  bool isEquivalent(tama::EqOp a, tama::EqOp b) {
    return unionFind.isEquivalent(a, b);
  }

private:
  llvm::EquivalenceClasses<tama::EqOp> unionFind;
};

#define GEN_PASS_DEF_TAMATCHTESTPASS
#include "TamatchPasses.h.inc"

namespace {
struct TamatchTestPass : public impl::TamatchTestPassBase<TamatchTestPass> {
  using impl::TamatchTestPassBase<TamatchTestPass>::TamatchTestPassBase;

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<mlir::pdl_interp::PDLInterpDialect>();
  }

  void runOnOperation() final {
    ModuleOp module = getOperation();

    // Load pattern and IR modules from input
    ModuleOp patternModule = module.lookupSymbol<ModuleOp>(
        StringAttr::get(module->getContext(), "patterns"));
    ModuleOp irModule = module.lookupSymbol<ModuleOp>(
        StringAttr::get(module->getContext(), "ir"));

    if (!patternModule || !irModule)
      return;

    RewritePatternSet patternList(module->getContext());

    // Process the pattern module
    patternModule.getOperation()->remove();
    PDLPatternModule pdlPattern(patternModule);

    EqOpUnionFind uf{};

    // Register custom rewrite functions
    pdlPattern.registerRewriteFunction("get_eq_vals", getEqVals);
    pdlPattern.registerRewriteFunction("get_eq_result", getEqResult);
    pdlPattern.registerRewriteFunction("union", [&uf](PatternRewriter &rewriter,
                                                      PDLResultList &results,
                                                      ArrayRef<PDLValue> args) {
      assert(args.size() == 2 && "union expects 2 arguments");

      PDLValue arg0 = args[0];
      PDLValue arg1 = args[1];

      // Value, Value
      if (arg0.isa<Value>() && arg1.isa<Value>()) {
        uf.eqUnion(rewriter, arg0.cast<Value>(), arg1.cast<Value>());
      }
      // Operation*, ValueRange
      else if (arg0.isa<Operation *>() && arg1.isa<ValueRange>()) {
        uf.eqUnion(rewriter, arg0.cast<Operation *>(), arg1.cast<ValueRange>());
      }
      // ValueRange, ValueRange
      else if (arg0.isa<ValueRange>() && arg1.isa<ValueRange>()) {
        uf.eqUnion(rewriter, arg0.cast<ValueRange>(), arg1.cast<ValueRange>());
      } else {
        llvm_unreachable("union: unsupported argument types");
      }
      return success();
    });
    patternList.add(std::move(pdlPattern));

    // Apply patterns greedily to the IR module
    (void)applyPatternsGreedily(irModule.getBodyRegion(),
                                std::move(patternList));
  }
};
} // namespace
} // namespace mlir::tamatch

#define GEN_PASS_REGISTRATION
#include "TamatchPasses.h.inc"

namespace mlir::tamatch {
void registerPasses() { registerTamatchTestPass(); }
} // namespace mlir::tamatch
