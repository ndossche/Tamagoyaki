//===- TamagoyakiCAPI.h - CAPI for tamagoyaki dialect -------------------*- C -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef TAMAGOYAKI_C_DIALECTS_H
#define TAMAGOYAKI_C_DIALECTS_H

#include "mlir-c/IR.h"

#ifdef __cplusplus
extern "C" {
#endif

MLIR_DECLARE_CAPI_DIALECT_REGISTRATION(Tamagoyaki, tamagoyaki);

//===---------------------------------------------------------------------===//
// CustomType
//===---------------------------------------------------------------------===//

MLIR_CAPI_EXPORTED bool mlirTypeIsATamagoyakiCustomType(MlirType type);

MLIR_CAPI_EXPORTED MlirType mlirTamagoyakiCustomTypeGet(MlirContext ctx, MlirStringRef value);

#ifdef __cplusplus
}
#endif

#endif // TAMAGOYAKI_C_DIALECTS_H
