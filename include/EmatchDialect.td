//===- EmatchDialect.td - Ematch dialect ----*- tablegen -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef EMATCH_DIALECT
#define EMATCH_DIALECT

include "mlir/IR/OpBase.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Pass/PassBase.td"

//===----------------------------------------------------------------------===//
// Ematch dialect definition.
//===----------------------------------------------------------------------===//

def Ematch_Dialect : Dialect {
    let name = "ematch";
    let summary = "An ematch out-of-tree MLIR dialect.";
    let description = [{
        This dialect is an example of an out-of-tree MLIR dialect designed to
        illustrate the basic setup required to develop MLIR-based tools without
        working inside of the LLVM source tree.
    }];
    let cppNamespace = "::mlir::ematch";

    let useDefaultTypePrinterParser = 1;
}

//===----------------------------------------------------------------------===//
// Base ematch operation definition.
//===----------------------------------------------------------------------===//

class Ematch_Op<string mnemonic, list<Trait> traits = []> :
        Op<Ematch_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// Ematch Operations
//===----------------------------------------------------------------------===//

def Ematch_FooOp : Ematch_Op<"foo", [Pure, SameOperandsAndResultType]> {
    let summary = "Illustrates how to define an operation.";
    let description = [{
        The `ematch.foo` operation illustrates how to define a new
        operation in a dialect. It uses an operation trait to declare that it
        has no side effects.

        This operation takes an integer argument and returns an integer.

        Example:

        ```mlir
        %0 = arith.constant 2 : i32
        // Apply the foo operation to %0
        %1 = ematch.foo %0 : i32
        ```
    }];

    let arguments = (ins I32:$input);
    let results = (outs I32:$res);

    let assemblyFormat = [{
        $input attr-dict `:` type($input)
    }];
}

//===----------------------------------------------------------------------===//
// Ematch Passes
//===----------------------------------------------------------------------===//

def EmatchSaturatePass : Pass<"ematch-saturate", "::mlir::ModuleOp"> {
    let summary = "Run equality saturation on the program with the provided patterns.";
    let description = [{
        This pass applies pattern rewriting to operations in the ematch dialect.
        It serves as an example of implementing a transformation pass.
    }];
    let dependentDialects = ["::mlir::ematch::EmatchDialect"];
}

def EmatchSaturateBenchmarkPass : Pass<"ematch-saturate-benchmark", "::mlir::ModuleOp"> {
    let summary = "Run equality saturation N times for benchmarking/profiling.";
    let description = [{
        This pass runs the ematch-saturate pass N times, cloning the IR module
        each time to ensure fresh input. Useful for profiling and benchmarking.
    }];
    let dependentDialects = ["::mlir::ematch::EmatchDialect"];
}

#endif // EMATCH_DIALECT
