//===- TamagoyakiDialect.h - Tamagoyaki dialect ------------------*- C++
//-*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef TAMAGOYAKI_TAMADIALECT_H
#define TAMAGOYAKI_TAMADIALECT_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"

#include <memory>

#include "TamagoyakiDialect.h.inc"

#define GET_OP_CLASSES
#include "TamagoyakiOps.h.inc"

namespace mlir::tama {
#define GEN_PASS_DECL
#include "TamagoyakiPasses.h.inc"

#define GEN_PASS_REGISTRATION
#include "TamagoyakiPasses.h.inc"
} // namespace mlir::tama

#define GET_TYPEDEF_CLASSES
#include "TamagoyakiTypes.h.inc"

#endif // TAMAGOYAKI_TAMADIALECT_H
