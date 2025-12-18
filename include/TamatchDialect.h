//===- TamatchDialect.h - Tamatch dialect -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef TAMATCH_TAMATCHDIALECT_H
#define TAMATCH_TAMATCHDIALECT_H

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include <memory>

#include "TamatchDialect.h.inc"

#define GET_OP_CLASSES
#include "TamatchOps.h.inc"

#endif // TAMATCH_TAMATCHDIALECT_H
