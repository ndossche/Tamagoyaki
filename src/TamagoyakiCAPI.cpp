//===- Dialects.cpp - CAPI for dialects -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamagoyakiCAPI.h"

#include "TamagoyakiDialect.h"

#include "mlir/CAPI/Registration.h"

MLIR_DEFINE_CAPI_DIALECT_REGISTRATION(Tamagoyaki, tamagoyaki,
                                      mlir::tamagoyaki::TamagoyakiDialect)

//===---------------------------------------------------------------------===//
// CustomType
//===---------------------------------------------------------------------===//

bool mlirTypeIsATamagoyakiCustomType(MlirType type) {
  return llvm::isa<mlir::tamagoyaki::CustomType>(unwrap(type));
}

MlirType mlirTamagoyakiCustomTypeGet(MlirContext ctx, MlirStringRef value) {
  return wrap(mlir::tamagoyaki::CustomType::get(unwrap(ctx), unwrap(value)));
}
