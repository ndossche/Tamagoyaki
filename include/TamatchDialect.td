//===- TamatchDialect.td - Tamatch dialect ----*- tablegen -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef TAMATCH_DIALECT
#define TAMATCH_DIALECT

include "mlir/IR/OpBase.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Pass/PassBase.td"

//===----------------------------------------------------------------------===//
// Tamatch dialect definition.
//===----------------------------------------------------------------------===//

def Tamatch_Dialect : Dialect {
    let name = "tamatch";
    let summary = "A tamatch out-of-tree MLIR dialect.";
    let description = [{
        This dialect is an example of an out-of-tree MLIR dialect designed to
        illustrate the basic setup required to develop MLIR-based tools without
        working inside of the LLVM source tree.
    }];
    let cppNamespace = "::mlir::tamatch";

    let useDefaultTypePrinterParser = 1;
}

//===----------------------------------------------------------------------===//
// Base tamatch operation definition.
//===----------------------------------------------------------------------===//

class Tamatch_Op<string mnemonic, list<Trait> traits = []> :
        Op<Tamatch_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// Tamatch Operations
//===----------------------------------------------------------------------===//

def Tamatch_FooOp : Tamatch_Op<"foo", [Pure, SameOperandsAndResultType]> {
    let summary = "Illustrates how to define an operation.";
    let description = [{
        The `tamatch.foo` operation illustrates how to define a new
        operation in a dialect. It uses an operation trait to declare that it
        has no side effects.

        This operation takes an integer argument and returns an integer.

        Example:

        ```mlir
        %0 = arith.constant 2 : i32
        // Apply the foo operation to %0
        %1 = tamatch.foo %0 : i32
        ```
    }];

    let arguments = (ins I32:$input);
    let results = (outs I32:$res);

    let assemblyFormat = [{
        $input attr-dict `:` type($input)
    }];
}

#endif // TAMATCH_DIALECT
