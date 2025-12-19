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
#include "mlir/IR/Value.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
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
  if (val.hasOneUse() && dyn_cast<tama::EqOp>(*val.user_begin())) {
    return llvm::to_vector(val.user_begin()->getOperands());
  }
  return {val};
}

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

    // Register custom rewrite functions
    pdlPattern.registerRewriteFunction("get_eq_vals", getEqVals);
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
