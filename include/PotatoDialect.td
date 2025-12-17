//===- PotatoDialect.td - Potato dialect -----------------*- tablegen -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef POTATO_DIALECT
#define POTATO_DIALECT

include "mlir/IR/OpBase.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Interfaces/ControlFlowInterfaces.td" 
include "mlir/Pass/PassBase.td"
include "mlir/IR/AttrTypeBase.td"

//===----------------------------------------------------------------------===//
// Potato dialect definition.
//===----------------------------------------------------------------------===//

def Potato_Dialect : Dialect {
    let name = "potato";
    let summary = "A potato out-of-tree MLIR dialect.";
    let description = [{
        This dialect is an example of an out-of-tree MLIR dialect designed to
        illustrate the basic setup required to develop MLIR-based tools without
        working inside of the LLVM source tree.
    }];
    let cppNamespace = "::mlir::potato";

    let useDefaultTypePrinterParser = 1;
    let extraClassDeclaration = [{
        void registerTypes();
    }];
}

//===----------------------------------------------------------------------===//
// Base potato operation definition.
//===----------------------------------------------------------------------===//

class Potato_Op<string mnemonic, list<Trait> traits = []> :
        Op<Potato_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// Potato Operations
//===----------------------------------------------------------------------===//

def Potato_FooOp : Potato_Op<"foo", [Pure, SameOperandsAndResultType]> {
    let summary = "Illustrates how to define an operation.";
    let description = [{
        The `potato.foo` operation illustrates how to define a new
        operation in a dialect. It uses an operation trait to declare that it
        has no side effects.

        This operation takes an integer argument and returns an integer.

        Example:

        ```mlir
        %0 = arith.constant 2 : i32
        // Apply the foo operation to %0
        %1 = potato.foo %0 : i32
        ```
    }];

    let arguments = (ins I32:$input);
    let results = (outs I32:$res);

    let assemblyFormat = [{
        $input attr-dict `:` type($input)
    }];
}

// definition of the YieldOp (eqsat.yield)
def Potato_YieldOp : Potato_Op<"yield", [
    Terminator, 
    HasParent<"EGraphOp">
]> {
    let summary = "Yield values from an egraph region";
    let description = [{
        The `potato.yield` operation yields values from a `potato.egraph` region.
        It serves as the terminator for the egraph block.
    }];

    let arguments = (ins Variadic<AnyType>:$values);
    
    let assemblyFormat = [{
        $values `:` type($values) attr-dict
    }];
    
    // This allows SingleBlockImplicitTerminator to build 'potato.yield'
    // without arguments when closing the block.
    let builders = [
        OpBuilder<(ins), [{ return; }]>
    ];

}

def Potato_EGraphOp : Potato_Op<"egraph", [
    // Ensures the region has one block and ends with a potato.yield
    SingleBlockImplicitTerminator<"YieldOp">,
 ]> {
    let summary = "An e-graph region operation";
    let description = [{
        The `potato.egraph` operation contains a single-block region in which
        equivalence classes and other operations are defined. Importantly,
        operations in the region are unordered, value uses can come before
        their definitions.
    }];

    let results = (outs Variadic<AnyType>:$outputs);

    let regions = (region SizedRegion<1>:$body);

    let assemblyFormat = [{
        `->` type($outputs) $body attr-dict
    }];
}

def Potato_EqOp : Potato_Op<"eq", [
    Pure, 
    SameOperandsAndResultType,
]> {
    let summary = "An e-class representing a set of equivalent values";
    let description = [{
        An e-class operation representing a set of equivalent values.
    }];

    let arguments = (ins 
        Variadic<AnyType>:$inputs,
        OptionalAttr<I64Attr>:$min_cost_index
    );

    let results = (outs AnyType:$result);

    let assemblyFormat = [{
        $inputs attr-dict `:` type($result)
    }];

    // Verify the following:
    // 1. At least one operand.
    // 2. Operands cannot be result of other EClassOp.
    // 3. Operands must be used only by this EClassOp.
    let hasVerifier = 1; 
}

//===----------------------------------------------------------------------===//
// Other Definitions
//===----------------------------------------------------------------------===//

def PotatoInsertEgraph : Pass<"potato-insert-egraph", "::mlir::ModuleOp"> {
  let summary = "Replaces a function body with a potato.egraph operation.";
  let dependentDialects = ["::mlir::potato::PotatoDialect", "::mlir::func::FuncDialect"];
}
  

def PotatoSwitchBarFoo: Pass<"potato-switch-bar-foo", "::mlir::ModuleOp"> {
  let summary = "Switches the name of a FuncOp named `bar` to `foo` and folds.";
  let description = [{
    Switches the name of a FuncOp named `bar` to `foo` and folds.
    ```
    func.func @bar() {
      return
    }
    // Gets transformed to:
    func.func @foo() {
      return
    }
    ```
  }];
}

class Potato_Type<string name, string typeMnemonic, list<Trait> traits = []>
    : TypeDef<Potato_Dialect, name, traits> {
  let mnemonic = typeMnemonic;
}

def Potato_CustomType : Potato_Type<"Custom", "custom"> {
    let summary = "Potato custom type";
    let description = "Custom type in potato dialect";
    let parameters = (ins StringRefParameter<"the custom value">:$value);
    let assemblyFormat = "`<` $value `>`";
}

#endif // POTATO_DIALECT
