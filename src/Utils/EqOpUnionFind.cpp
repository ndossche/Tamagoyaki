//===- EqOpUnionFind.cpp - Union-find data structure for EqOp ---*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Utils/EqOpUnionFind.h"
#include "TamagoyakiDialect.h"
#include "Utils/HashConsPatternRewriter.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include <cassert>
#include <utility>

#define DEBUG_TYPE "tamatch"

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

  worklist.push_back(leader);
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

bool EqOpUnionFind::rebuild(PatternRewriter &rewriter) {
  LLVM_DEBUG({
    llvm::dbgs() << "Starting rebuild. Content of worklist: \n";
    for (tama::EqOp c : worklist) {
      llvm::dbgs() << "\t" << c << "\n";
    }
  });

  if (worklist.empty())
    return false;

  while (!worklist.empty()) {
    // Create an ordered set of unique leaders from the worklist
    llvm::SetVector<tama::EqOp> todo;
    for (tama::EqOp c : worklist) {
      todo.insert(*unionFind.findLeader(c));
    }
    worklist.clear();

    // Repair each unique leader
    for (tama::EqOp c : todo) {
      repair(rewriter, c);
    }
  }
  return true;
}

void EqOpUnionFind::repair(PatternRewriter &rewriter, tama::EqOp eqOp) {
  // Get the canonical leader
  eqOp = *unionFind.findLeader(eqOp);

  // Create scoped map for hash-consing parent operations
  ScopedMapTy uniqueParents;
  ScopedMapTy::ScopeTy scope(uniqueParents);

  // Collect parent operations (operations that use this eclass's result)
  // Use SetVector to maintain insertion order while deduplicating
  llvm::SetVector<Operation *> parentOps;
  for (OpOperand &use : eqOp.getResult().getUses()) {
    parentOps.insert(use.getOwner());
  }

  for (Operation *op1 : parentOps) {
    // Skip EqOp operations - they're the eclasses themselves
    if (isa<tama::EqOp>(op1))
      continue;

    // Look up in hash-consing table to find equivalent operation
    Operation *op2 = uniqueParents.lookup(op1);

    if (op2) {
      // Found an equivalent operation - need to merge their eclasses

      // Collect eclass pairs before replacement (since replacement invalidates
      // uses)
      SmallVector<std::pair<tama::EqOp, tama::EqOp>> eclassPairs;
      for (auto [res1, res2] :
           llvm::zip(op1->getResults(), op2->getResults())) {
        tama::EqOp eclass1 = getEqOp(rewriter, res1);
        tama::EqOp eclass2 = getEqOp(rewriter, res2);
        eclassPairs.emplace_back(eclass1, eclass2);
      }

      // Replace op1 with op2's results and erase op1
      rewriter.replaceOp(op1, op2->getResults());

      // Process each result's eclass pair
      for (auto [eclass1, eclass2] : eclassPairs) {
        if (eclass1 == eclass2) {
          // Same eclass - just deduplicate operands
          SmallPtrSet<Value, 8> seen;
          SmallVector<Value> uniqueOperands;
          for (Value operand : eclass1->getOperands()) {
            if (seen.insert(operand).second)
              uniqueOperands.push_back(operand);
          }
          eclass1->setOperands(uniqueOperands);
        } else {
          // Different eclasses - union them (this adds to worklist)
          eqUnion(rewriter, eclass1.getResult(), eclass2.getResult());
        }
      }
    } else {
      // No equivalent found, register this op
      uniqueParents.insert(op1, op1);
    }
  }
}
