//===- TamatchDialect.cpp - Tamatch dialect -----*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamatchDialect.h"

#include "TamagoyakiDialect.h"
#include "mlir/Bytecode.h"
#include "mlir/CSE.h"
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
#include "mlir/MutableScopedHashTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassOptions.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Rewrite/PatternApplicator.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/RecyclingAllocator.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <ostream>
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

    erase(other); // remove from union-find
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

  void erase(tama::EqOp op) { unionFind.erase(op); }

private:
  llvm::EquivalenceClasses<tama::EqOp> unionFind;
};

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

using AllocatorTy = llvm::RecyclingAllocator<
    llvm::BumpPtrAllocator,
    mlir::tamatch::MutableScopedHashTableVal<Operation *, Operation *>>;

using ScopedMapTy = MutableScopedHashTable<Operation *, Operation *,
                                           SimpleOperationInfo, AllocatorTy>;

class HashConsPatternRewriter : public PatternRewriter {
public:
  using PatternRewriter::PatternRewriter;

  void startOpModification(Operation *op) override {
    LLVM_DEBUG(llvm::dbgs()
               << "operation being modified (start): " << *op << "\n");
    hashcons.erase(op);
  }

  void cancelOpModification(Operation *op) override {
    LLVM_DEBUG(llvm::dbgs()
               << "operation being modified (cancel): " << *op << "\n");
    hashcons.insert(op, op);
  }

  void finalizeOpModification(Operation *op) override {
    LLVM_DEBUG(llvm::dbgs()
               << "operation being modified (finalize): " << *op << "\n");
    hashcons.insert(op, op);
    if (auto *rewriteListener = dyn_cast_if_present<Listener>(listener))
      rewriteListener->notifyOperationModified(op);
  }

  ScopedMapTy hashcons;
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
    HashConsPatternRewriter rewriter(module.getContext());
    ScopedMapTy &hashcons = rewriter.hashcons;
    ScopedMapTy::ScopeTy scope(hashcons);

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
      rewriter.setListener(&guard);
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

      // Execute the rewrite. This will trigger the registered "union" callback.
      // We pass the same bytecodeState so it can access captured values.
      (void)bytecode->rewrite(rewriter, pm.matchResult, bytecodeState);
    }
  }
};
} // namespace
} // namespace mlir::tamatch

#define GEN_PASS_REGISTRATION
#include "TamatchPasses.h.inc"

namespace mlir::tamatch {
void registerPasses() { registerTamatchSaturatePass(); }
} // namespace mlir::tamatch
