//- HashConsPatternRewriter.h - Hash consing for pattern rewriting -*- C++ -*-//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef TAMAGOYAKI_SRC_UTILS_HASHCONSPATTERNREWRITER_H
#define TAMAGOYAKI_SRC_UTILS_HASHCONSPATTERNREWRITER_H

#include "MutableScopedHashTable.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/PatternMatch.h"
#include "vendor/mlir/SimpleOperationInfo.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/RecyclingAllocator.h"

namespace mlir::tamatch {

/// Allocator type for hash consing
using AllocatorTy = llvm::RecyclingAllocator<
    llvm::BumpPtrAllocator,
    mlir::tamatch::MutableScopedHashTableVal<Operation *, Operation *>>;

/// Scoped hash table type for operation deduplication
using ScopedMapTy = MutableScopedHashTable<Operation *, Operation *,
                                           SimpleOperationInfo, AllocatorTy>;

/// Pattern rewriter with hash consing support
class HashConsPatternRewriter : public PatternRewriter {
  struct HashConsListener : public RewriterBase::ForwardingListener {
    HashConsListener(ScopedMapTy &hashcons,
                     OpBuilder::Listener *listener = nullptr)
        : ForwardingListener(listener), hashcons(hashcons),
          underlyingListener(listener) {}

    void setUnderlyingListener(OpBuilder::Listener *listener);

  private:
    ScopedMapTy &hashcons;
    OpBuilder::Listener *underlyingListener = nullptr;
  };

public:
  explicit HashConsPatternRewriter(MLIRContext *ctx);

  void startOpModification(Operation *op) override;
  void cancelOpModification(Operation *op) override;
  void finalizeOpModification(Operation *op) override;

  /// Set an underlying listener that will receive forwarded notifications
  /// in addition to the hashcons listener's own handling.
  void setUnderlyingListener(OpBuilder::Listener *listener);

  ScopedMapTy hashcons;

private:
  HashConsListener hashConsListener;
};

} // namespace mlir::tamatch

#endif // TAMAGOYAKI_SRC_UTILS_HASHCONSPATTERNREWRITER_H
