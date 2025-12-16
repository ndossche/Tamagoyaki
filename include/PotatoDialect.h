//===- PotatoDialect.h - Potato dialect -----------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef POTATO_POTATODIALECT_H
#define POTATO_POTATODIALECT_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"

#include <memory>

#include "PotatoDialect.h.inc"

#define GET_OP_CLASSES
#include "PotatoOps.h.inc"

namespace mlir::potato {
#define GEN_PASS_DECL
#include "PotatoPasses.h.inc"

#define GEN_PASS_REGISTRATION
#include "PotatoPasses.h.inc"
} // namespace mlir::potato

#define GET_TYPEDEF_CLASSES
#include "PotatoTypes.h.inc"

#endif // POTATO_POTATODIALECT_H
