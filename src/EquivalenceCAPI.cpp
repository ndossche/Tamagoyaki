//===- Dialects.cpp - CAPI for dialects -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EquivalenceCAPI.h"

#include "EquivalenceDialect.h"

#include "mlir-c/IR.h"
#include "mlir-c/Support.h"
#include "mlir/CAPI/IR.h"
#include "mlir/CAPI/Registration.h"
#include "mlir/CAPI/Support.h"
#include "llvm/Support/Casting.h"

MLIR_DEFINE_CAPI_DIALECT_REGISTRATION(Equivalence, equivalence,
                                      mlir::equivalence::EquivalenceDialect)

//===---------------------------------------------------------------------===//
// CustomType
//===---------------------------------------------------------------------===//

bool mlirTypeIsAEquivalenceCustomType(MlirType type) {
  return llvm::isa<mlir::equivalence::CustomType>(unwrap(type));
}

MlirType mlirEquivalenceCustomTypeGet(MlirContext ctx, MlirStringRef value) {
  return wrap(mlir::equivalence::CustomType::get(unwrap(ctx), unwrap(value)));
}
