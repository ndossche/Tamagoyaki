//===- TamatchDialect.cpp - Tamatch dialect -----*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamatchDialect.h"

#include "TamagoyakiDialect.h"
#include "Utils/EqOpUnionFind.h"
#include "Utils/HashConsPatternRewriter.h"
#include "Utils/MutableScopedHashTable.h"
#include "mlir/Dialect/PDLInterp/IR/PDLInterp.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
// IWYU pragma: no_include "mlir/IR/PDLPatternMatch.h.inc"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Rewrite/PatternApplicator.h"
#include "mlir/Support/LLVM.h"
#include "vendor/mlir/Bytecode.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>
#include <chrono>
#include <utility>

#define DEBUG_TYPE "tamatch"

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

#define GEN_PASS_DEF_TAMATCHSATURATEPASS
#include "TamatchPasses.h.inc"

namespace {

struct NoEraseGuard : public RewriterBase::Listener {
  void notifyOperationErased(Operation *op) override {
    if (!dyn_cast<tama::EqOp>(*op)) {
      op->emitError("Operations cannot be erased during equality saturation.");
      llvm_unreachable("Operation erased against expectation.");
    }
  }

  void notifyOperationModified(Operation *op) override {
    LLVM_DEBUG(llvm::dbgs() << "notifyOperationModified: " << *op << "\n");
  }
};

struct TamatchSaturatePass
    : public impl::TamatchSaturatePassBase<TamatchSaturatePass> {
  using impl::TamatchSaturatePassBase<
      TamatchSaturatePass>::TamatchSaturatePassBase;

  TamatchSaturatePass() = default;
  TamatchSaturatePass(const TamatchSaturatePass &pass)
      : TamatchSaturatePassBase(pass) {}

  Option<bool> verifyNoErase{
      *this, "verify-no-erase",
      llvm::cl::desc("Whether to throw an error when an operation is removed "
                     "during equality saturation, defaults to true."),
      llvm::cl::init(true)};
  Option<int> maxIters{
      *this, "max-iters",
      llvm::cl::desc("Maximum number of iterations before equality saturation "
                     "times out."),
      llvm::cl::init(4)};

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<mlir::pdl_interp::PDLInterpDialect>();
  }

  void runOnOperation() final {
    auto startTime = std::chrono::high_resolution_clock::now();

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
    HashConsPatternRewriter rewriter(module.getContext());
    ScopedMapTy &hashcons = rewriter.hashcons;
    ScopedMapTy::ScopeTy rootScope(hashcons);

    irModule.walk([&](Operation *op) {
      if (dyn_cast<tama::EqOp>(*op)) {
        return;
      }
      hashcons.insert(op, op);
    });

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
    pdlPattern.registerRewriteFunction(
        "dedup", [&hashcons](PatternRewriter &rewriter, Operation *op) {
          if (auto *existing = hashcons.lookup(op)) {
            LLVM_DEBUG(llvm::dbgs()
                       << "deduplicating operation: " << *op << "\n");
            assert(existing != op);
            rewriter.eraseOp(op);
            return existing;
          }
          LLVM_DEBUG(llvm::dbgs() << "no duplicate, inserting into hashcons: "
                                  << *op << "\n");
          hashcons.insert(op, op);
          return op;
        });
    patternList.add(std::move(pdlPattern));

    FrozenRewritePatternSet frozenPatterns(std::move(patternList));

    NoEraseGuard guard;
    if (verifyNoErase) {
      rewriter.setUnderlyingListener(&guard);
    }

    // Structure to hold deferred matches
    struct PendingMatch {
      Operation *op;
      mlir::detail::PDLByteCode::MatchResult matchResult;
    };
    SmallVector<PendingMatch> allMatches;

    const auto *bytecode = frozenPatterns.getPDLByteCode();
    if (!bytecode) {
      // No PDL patterns found
      return;
    }

    // Initialize the mutable state for the bytecode interpreter.
    // This manages memory for matches. Crucially, we keep this alive
    // between the Match phase and the Rewrite phase.
    mlir::detail::PDLByteCodeMutableState bytecodeState;

    int nIters = 0;

    do {
      nIters++;
      LLVM_DEBUG(llvm::dbgs() << "Equality saturation: starting iteration "
                              << nIters << "\n");
      if (nIters > maxIters) {
        break;
      }

      bytecode->initializeMutableState(bytecodeState);

      // Walk the IR and collect ALL matches for ALL operations.
      irModule.walk([&](Operation *op) {
        SmallVector<mlir::detail::PDLByteCode::MatchResult, 4> opMatches;

        // Execute the bytecode matcher.
        // matches are allocated in bytecodeState and pointers are stored in
        // opMatches.
        bytecode->match(op, rewriter, opMatches, bytecodeState);

        for (auto &match : opMatches) {
          allMatches.push_back({op, std::move(match)});
        }
      });

      // Apply rewrites for all collected matches.
      for (const auto &pm : allMatches) {
        // Set insertion point to the matched operation (standard PDL behavior)
        rewriter.setInsertionPoint(pm.op);

        // Execute the rewrite. This will trigger the registered "union"
        // callback. We pass the same bytecodeState so it can access captured
        // values.
        (void)bytecode->rewrite(rewriter, pm.matchResult, bytecodeState);
      }
      bytecodeState.cleanupAfterMatchAndRewrite();
    } while (uf.rebuild(rewriter));
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);
    LLVM_DEBUG(llvm::dbgs()
               << "TamatchSaturatePass took " << duration.count() << " µs\n");
  }
};
} // namespace
} // namespace mlir::tamatch

#define GEN_PASS_REGISTRATION
#include "TamatchPasses.h.inc"

namespace mlir::tamatch {
void registerPasses() { registerTamatchSaturatePass(); }
} // namespace mlir::tamatch
