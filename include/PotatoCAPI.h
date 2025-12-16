//===- PotatoCAPI.h - CAPI for potato dialect -------------------*- C -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef POTATO_C_DIALECTS_H
#define POTATO_C_DIALECTS_H

#include "mlir-c/IR.h"

#ifdef __cplusplus
extern "C" {
#endif

MLIR_DECLARE_CAPI_DIALECT_REGISTRATION(Potato, potato);

//===---------------------------------------------------------------------===//
// CustomType
//===---------------------------------------------------------------------===//

MLIR_CAPI_EXPORTED bool mlirTypeIsAPotatoCustomType(MlirType type);

MLIR_CAPI_EXPORTED MlirType mlirPotatoCustomTypeGet(MlirContext ctx, MlirStringRef value);

#ifdef __cplusplus
}
#endif

#endif // POTATO_C_DIALECTS_H
