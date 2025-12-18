//===- TamatchDialect.cpp - Tamatch dialect -----*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamatchDialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/StringRef.h"

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

void TamatchDialect::printType(Type type, DialectAsmPrinter &printer) const {
  printer << "unknown";
}

//===----------------------------------------------------------------------===//
// Tamatch ops
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "TamatchOps.cpp.inc"
