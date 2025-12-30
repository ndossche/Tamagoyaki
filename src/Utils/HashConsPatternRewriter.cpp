//- HashConsPatternRewriter.cpp - Hash consing for pattern rewriting -*-C++ -*//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Utils/HashConsPatternRewriter.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "tamatch"

using namespace mlir;
using namespace mlir::tamatch;

void HashConsPatternRewriter::HashConsListener::notifyOperationErased(
    Operation *op) {
  LLVM_DEBUG(llvm::dbgs() << "HashConsListener::notifyOperationErased: " << *op
                          << "\n");
  hashcons.erase(op);
  ForwardingListener::notifyOperationErased(op);
}

void HashConsPatternRewriter::HashConsListener::setUnderlyingListener(
    OpBuilder::Listener *listener) {
  underlyingListener = listener;
  // Reinitialize the base class with the new listener
  this->~HashConsListener();
  new (this) HashConsListener(hashcons, listener);
}

HashConsPatternRewriter::HashConsPatternRewriter(MLIRContext *ctx)
    : PatternRewriter(ctx), hashConsListener(hashcons) {
  setListener(&hashConsListener);
}

void HashConsPatternRewriter::startOpModification(Operation *op) {
  LLVM_DEBUG(llvm::dbgs() << "operation being modified (start): " << *op
                          << "\n");
  hashcons.erase(op);
}

void HashConsPatternRewriter::cancelOpModification(Operation *op) {
  LLVM_DEBUG(llvm::dbgs() << "operation being modified (cancel): " << *op
                          << "\n");
  hashcons.insert(op, op);
}

void HashConsPatternRewriter::finalizeOpModification(Operation *op) {
  LLVM_DEBUG(llvm::dbgs() << "operation being modified (finalize): " << *op
                          << "\n");
  hashcons.insert(op, op);
  PatternRewriter::finalizeOpModification(op);
}

void HashConsPatternRewriter::setUnderlyingListener(
    OpBuilder::Listener *listener) {
  hashConsListener.setUnderlyingListener(listener);
}
