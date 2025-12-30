//===- EqOpUnionFind.cpp - Union-find data structure for EqOp ---*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Utils/EqOpUnionFind.h"
#include "TamagoyakiDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include <cassert>

using namespace mlir;
using namespace mlir::tamatch;

SmallVector<Value> mlir::tamatch::getEqVals(PatternRewriter &rewriter,
                                            Value val) {
  if (auto eqOp = dyn_cast<tama::EqOp>(val.getDefiningOp())) {
    return llvm::to_vector(eqOp->getOperands());
  }
  return {val};
}

Value mlir::tamatch::getEqResult(PatternRewriter &rewriter, Value val) {
  if (auto eqOp =
          val.hasOneUse() ? dyn_cast<tama::EqOp>(*val.user_begin()) : nullptr) {
    return eqOp.getResult();
  }
  return val;
}

tama::EqOp mlir::tamatch::getEqOp(PatternRewriter &rewriter, Value val) {
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

void EqOpUnionFind::eqUnion(PatternRewriter &rewriter, Value a, Value b) {
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

  erase(other); // remove from union-find
  rewriter.eraseOp(other);
}

void EqOpUnionFind::eqUnion(PatternRewriter &rewriter, Operation *op,
                            ValueRange vals) {
  assert(op->getNumResults() == vals.size() &&
         "Operation result count must match value range size");
  for (auto [result, val] : llvm::zip(op->getResults(), vals))
    eqUnion(rewriter, result, val);
}

void EqOpUnionFind::eqUnion(PatternRewriter &rewriter, ValueRange a,
                            ValueRange b) {
  assert(a.size() == b.size() && "Value ranges must have equal size");
  for (auto [va, vb] : llvm::zip(a, b))
    eqUnion(rewriter, va, vb);
}

bool EqOpUnionFind::isEquivalent(tama::EqOp a, tama::EqOp b) {
  return unionFind.isEquivalent(a, b);
}

void EqOpUnionFind::erase(tama::EqOp op) { unionFind.erase(op); }
