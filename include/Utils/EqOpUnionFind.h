//===- EqOpUnionFind.h - Union-find data structure for EqOp -----*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef TAMAGOYAKI_SRC_UTILS_EQOPUNIONFIND_H
#define TAMAGOYAKI_SRC_UTILS_EQOPUNIONFIND_H

#include "TamagoyakiDialect.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/EquivalenceClasses.h"

namespace mlir::tamatch {

/// Helper function to get all values from an EqOp
SmallVector<mlir::Value> getEqVals(mlir::PatternRewriter &rewriter,
                                   mlir::Value val);

/// Helper function to get the result of an EqOp
mlir::Value getEqResult(mlir::PatternRewriter &rewriter, mlir::Value val);

/// Helper function to get or create an EqOp for a value
tama::EqOp getEqOp(mlir::PatternRewriter &rewriter, mlir::Value val);

/// Union-find data structure for managing equivalence classes of EqOp
class EqOpUnionFind {
public:
  /// Union two individual values
  void eqUnion(mlir::PatternRewriter &rewriter, mlir::Value a, mlir::Value b);

  /// Union an operation's results with corresponding values
  void eqUnion(mlir::PatternRewriter &rewriter, mlir::Operation *op,
               mlir::ValueRange vals);

  /// Union two value ranges pairwise
  void eqUnion(mlir::PatternRewriter &rewriter, mlir::ValueRange a,
               mlir::ValueRange b);

  /// Check if two values are in the same equivalence class
  bool isEquivalent(tama::EqOp a, tama::EqOp b);

  /// Erase an EqOp from the union-find
  void erase(tama::EqOp op);

  /// Repair the parents of each EqOp in the worklist.
  /// This also clears the worklist.
  /// Returns false when the worklist was empty, otherwise true.
  bool rebuild(PatternRewriter &rewriter);

  /// Repair e-graph by potentially deduplicating the parents of
  /// a merged EqOp.
  void repair(PatternRewriter &rewriter, tama::EqOp eqOp);

  /// List of EqOps whose parents potentially need to be repaired.
  SmallVector<tama::EqOp> worklist;

private:
  llvm::EquivalenceClasses<tama::EqOp> unionFind;
};

} // namespace mlir::tamatch

#endif // TAMAGOYAKI_SRC_UTILS_EQOPUNIONFIND_H
