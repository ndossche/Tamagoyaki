//===- ClassOpUnionFind.cpp - Union-find data structure for ClassOp ---*- C++
//-*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Utils/ClassOpUnionFind.h"
#include "EquivalenceDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"
#include "vendor/mlir/SimpleOperationInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include <cassert>
#include <utility>

#define DEBUG_TYPE "ematch"

using namespace mlir;
using namespace mlir::ematch;

SmallVector<Value> mlir::ematch::getClassVals(PatternRewriter &rewriter,
                                              Value val) {
  Operation *defOp = val.getDefiningOp();
  if (defOp == nullptr) {
    return {val};
  } else if (auto classOp = dyn_cast<equivalence::ClassOp>(defOp)) {
    return llvm::to_vector(classOp->getOperands());
  }
  return {val};
}

Value mlir::ematch::getClassRepresentative(PatternRewriter &rewriter,
                                           Value val) {
  return getClassVals(rewriter, val)[0];
}

Value mlir::ematch::getClassResult(PatternRewriter &rewriter, Value val) {
  if (val == nullptr) {
    return val;
  }
  if (auto classOp = val.hasOneUse()
                         ? dyn_cast<equivalence::ClassOp>(*val.user_begin())
                         : nullptr) {
    return classOp.getResult();
  }
  return val;
}

SmallVector<Value> mlir::ematch::getClassResults(PatternRewriter &rewriter,
                                                 ValueRange vals) {
  SmallVector<Value> results;
  results.reserve(vals.size());

  for (Value val : vals) {
    results.push_back(getClassResult(rewriter, val));
  }

  return results;
}

equivalence::ClassOp mlir::ematch::getClassOp(PatternRewriter &rewriter,
                                              Value val) {

  Operation *defOp = val.getDefiningOp();
  if (defOp != nullptr && dyn_cast<equivalence::ClassOp>(*defOp)) {
    return cast<equivalence::ClassOp>(*defOp);
  }
  if (auto classOp = val.hasOneUse()
                         ? dyn_cast<equivalence::ClassOp>(*val.user_begin())
                         : nullptr) {
    return classOp;
  }

  // If the value is not part of an eclass yet, create one
  OpBuilder builder(val.getContext());
  builder.setInsertionPointAfterValue(val);
  auto classOp = equivalence::ClassOp::create(
      builder, val.getLoc(), TypeRange{val.getType()}, ValueRange{val});
  rewriter.replaceUsesWithIf(
      val, classOp.getResult(),
      [&classOp](OpOperand &operand) { return operand.getOwner() != classOp; });
  return classOp;
}

void ClassOpUnionFind::classUnion(PatternRewriter &rewriter, Value a, Value b) {
  if (a == b) {
    return;
  }

  equivalence::ClassOp classA = getClassOp(rewriter, a);
  equivalence::ClassOp classB = getClassOp(rewriter, b);

  if (isEquivalent(classA, classB))
    return;

  // TODO: unionSets always treats the first argument as leader
  // this might lead to an unbalanced union-find?
  equivalence::ClassOp leader = *unionFind.unionSets(classA, classB);
  equivalence::ClassOp other = classB;

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

void ClassOpUnionFind::classUnion(PatternRewriter &rewriter, Operation *op,
                                  ValueRange vals) {
  assert(op->getNumResults() == vals.size() &&
         "Operation result count must match value range size");
  for (auto [result, val] : llvm::zip(op->getResults(), vals))
    classUnion(rewriter, result, val);
}

void ClassOpUnionFind::classUnion(PatternRewriter &rewriter, ValueRange a,
                                  ValueRange b) {
  assert(a.size() == b.size() && "Value ranges must have equal size");
  for (auto [va, vb] : llvm::zip(a, b))
    classUnion(rewriter, va, vb);
}

bool ClassOpUnionFind::isEquivalent(equivalence::ClassOp a,
                                    equivalence::ClassOp b) {
  return unionFind.isEquivalent(a, b);
}

void ClassOpUnionFind::erase(equivalence::ClassOp op) { unionFind.erase(op); }

equivalence::ClassOp ClassOpUnionFind::findLeader(equivalence::ClassOp c) {
  auto it = unionFind.findLeader(c);
  if (it == unionFind.member_end()) {
    return c;
  } else {
    assert(unionFind.contains(c));
    return *it;
  }
}

bool ClassOpUnionFind::rebuild(PatternRewriter &rewriter) {
  LLVM_DEBUG({
    llvm::dbgs() << "Starting rebuild. Worklist contains " << worklist.size()
                 << " classes\n";
  });

  if (worklist.empty())
    return false;

  while (!worklist.empty()) {
    // Create an ordered set of unique leaders from the worklist
    llvm::SetVector<equivalence::ClassOp> todo;
    for (equivalence::ClassOp c : worklist) {
      todo.insert(findLeader(c));
    }
    worklist.clear();

    // Repair each unique leader
    for (equivalence::ClassOp c : todo) {
      repair(rewriter, c);
    }
  }
  return true;
}

void ClassOpUnionFind::repair(PatternRewriter &rewriter,
                              equivalence::ClassOp classOp) {
  if (classOp->getBlock() == nullptr) {
    return;
  }
  classOp = findLeader(classOp);

  llvm::DenseMap<Operation *, Operation *, SimpleOperationInfo> uniqueParents;

  // Collect parent operations (operations that use this eclass's result)
  llvm::SetVector<Operation *> parentOps;
  for (OpOperand &use : classOp.getResult().getUses()) {
    parentOps.insert(use.getOwner());
  }

  // Collect pairs of duplicate operations to merge AFTER the loop
  SmallVector<std::pair<Operation *, Operation *>> toMerge;

  for (Operation *op1 : parentOps) {
    if (isa<equivalence::ClassOp>(op1))
      continue;

    Operation *op2 = uniqueParents.lookup(op1);

    if (op2) {
      // Found an equivalent operation - record for later merging
      toMerge.emplace_back(op1, op2);
    } else {
      uniqueParents[op1] = op1;
    }
  }

  // Now perform all merges after we're done with the hash map
  for (auto [op1, op2] : toMerge) {
    // Collect eclass pairs before replacement
    SmallVector<std::pair<equivalence::ClassOp, equivalence::ClassOp>>
        eclassPairs;
    for (auto [res1, res2] : llvm::zip(op1->getResults(), op2->getResults())) {
      equivalence::ClassOp eclass1 = getClassOp(rewriter, res1);
      equivalence::ClassOp eclass2 = getClassOp(rewriter, res2);
      eclassPairs.emplace_back(eclass1, eclass2);
    }

    rewriter.replaceOp(op1, op2->getResults());

    for (auto [eclass1, eclass2] : eclassPairs) {
      if (eclass1 == eclass2) {
        SmallPtrSet<Value, 8> seen;
        SmallVector<Value> uniqueOperands;
        for (Value operand : eclass1->getOperands()) {
          if (seen.insert(operand).second)
            uniqueOperands.push_back(operand);
        }
        eclass1->setOperands(uniqueOperands);
      } else {
        classUnion(rewriter, eclass1.getResult(), eclass2.getResult());
      }
    }
  }
}
