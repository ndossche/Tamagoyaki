//===- TamatchDialect.cpp - Tamatch dialect -----*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamatchDialect.h"

#include "EquivalenceDialect.h"
#include "Utils/ClassOpUnionFind.h"
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
#include "llvm/Support/Casting.h"
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
struct TamatchSaturatePass
    : public impl::TamatchSaturatePassBase<TamatchSaturatePass> {
  using impl::TamatchSaturatePassBase<
      TamatchSaturatePass>::TamatchSaturatePassBase;

  TamatchSaturatePass() = default;
  TamatchSaturatePass(const TamatchSaturatePass &pass)
      : TamatchSaturatePassBase(pass) {}

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

    ClassOpUnionFind uf{};
    HashConsPatternRewriter hashconsRewriter(module.getContext());

    irModule.walk([&](Operation *e) {
      equivalence::GraphOp graph = llvm::dyn_cast<equivalence::GraphOp>(*e);
      if (!graph) {
        return;
      }
      Region *region = &(graph.getBody());
      auto scope = hashconsRewriter.createRootScope(region);

      graph->walk([&scope](Operation *op) {
        if (dyn_cast<equivalence::ClassOp>(*op)) {
          return;
        }
        scope->insert(op, op);
      });
    });

    // Register custom rewrite functions
    pdlPattern.registerRewriteFunction("get_class_vals", getClassVals);
    pdlPattern.registerRewriteFunction("get_class_representative",
                                       getClassRepresentative);
    pdlPattern.registerRewriteFunction("get_class_result", getClassResult);
    pdlPattern.registerRewriteFunction("get_class_results", getClassResults);
    pdlPattern.registerRewriteFunction("union", [&uf](PatternRewriter &rewriter,
                                                      PDLResultList &results,
                                                      ArrayRef<PDLValue> args) {
      assert(args.size() == 2 && "union expects 2 arguments");

      PDLValue arg0 = args[0];
      PDLValue arg1 = args[1];

      // Value, Value
      if (arg0.isa<Value>() && arg1.isa<Value>()) {
        uf.classUnion(rewriter, arg0.cast<Value>(), arg1.cast<Value>());
      }
      // Operation*, ValueRange
      else if (arg0.isa<Operation *>() && arg1.isa<ValueRange>()) {
        uf.classUnion(rewriter, arg0.cast<Operation *>(),
                      arg1.cast<ValueRange>());
      }
      // ValueRange, ValueRange
      else if (arg0.isa<ValueRange>() && arg1.isa<ValueRange>()) {
        uf.classUnion(rewriter, arg0.cast<ValueRange>(),
                      arg1.cast<ValueRange>());
      } else {
        llvm_unreachable("union: unsupported argument types");
      }
      return success();
    });
    pdlPattern.registerRewriteFunction(
        "dedup", [&hashconsRewriter](PatternRewriter &rewriter, Operation *op) {
          if (Operation *existing = hashconsRewriter.lookup(op)) {
            LLVM_DEBUG(llvm::dbgs()
                       << "deduplicating operation: " << *op << "\n");
            assert(existing != op);
            rewriter.eraseOp(op);
            return existing;
          }
          LLVM_DEBUG(llvm::dbgs() << "no duplicate, inserting into hashcons: "
                                  << *op << "\n");
          hashconsRewriter.insert(op);
          return op;
        });
    patternList.add(std::move(pdlPattern));

    FrozenRewritePatternSet frozenPatterns(std::move(patternList));

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
      if (nIters > maxIters) {
        break;
      }
      LLVM_DEBUG(llvm::dbgs() << "Equality saturation: starting iteration "
                              << nIters << "\n");

      bytecode->initializeMutableState(bytecodeState);

      int i = 0;
      // Walk the IR and collect ALL matches for ALL operations.
      irModule.walk([&](Operation *op) {
        auto dialect = op->getDialect();
        if (dialect != nullptr &&
            isa<equivalence::EquivalenceDialect>(op->getDialect()))
          return;

        SmallVector<mlir::detail::PDLByteCode::MatchResult, 4> opMatches;

        // Execute the bytecode matcher.
        // matches are allocated in bytecodeState and pointers are stored in
        // opMatches.
        bytecode->match(op, hashconsRewriter, opMatches, bytecodeState);

        for (auto &match : opMatches) {
          allMatches.push_back({op, std::move(match)});

          i += 1;
          LLVM_DEBUG({
            llvm::dbgs() << "Recording rewrite " << i << " at root " << *op
                         << "\n";
          });
        }
      });

      i = 0;
      // Apply rewrites for all collected matches.
      for (const auto &pm : allMatches) {
        // Set insertion point to the matched operation (standard PDL behavior)
        hashconsRewriter.setInsertionPoint(pm.op);
        i += 1;
        LLVM_DEBUG({ llvm::dbgs() << "Applying rewrite " << i << "\n"; });

        // Execute the rewrite. This will trigger the registered "union"
        // callback. We pass the same bytecodeState so it can access captured
        // values.
        (void)bytecode->rewrite(hashconsRewriter, pm.matchResult,
                                bytecodeState);
      }
      allMatches.clear();
      bytecodeState.cleanupAfterMatchAndRewrite();
    } while (uf.rebuild(hashconsRewriter));

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
