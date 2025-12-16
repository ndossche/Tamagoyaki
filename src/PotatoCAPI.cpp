//===- Dialects.cpp - CAPI for dialects -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PotatoCAPI.h"

#include "PotatoDialect.h"

#include "mlir/CAPI/Registration.h"

MLIR_DEFINE_CAPI_DIALECT_REGISTRATION(Potato, potato,
                                      mlir::potato::PotatoDialect)

//===---------------------------------------------------------------------===//
// CustomType
//===---------------------------------------------------------------------===//

bool mlirTypeIsAPotatoCustomType(MlirType type) {
  return llvm::isa<mlir::potato::CustomType>(unwrap(type));
}

MlirType mlirPotatoCustomTypeGet(MlirContext ctx, MlirStringRef value) {
  return wrap(mlir::potato::CustomType::get(unwrap(ctx), unwrap(value)));
}
