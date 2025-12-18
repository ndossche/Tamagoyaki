//===- TamagoyakiExtension.cpp - Extension module--------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamagoyakiCAPI.h"

#include "mlir/Bindings/Python/PybindAdaptors.h"
using namespace py::literals;

using namespace mlir::python::adaptors;

PYBIND11_MODULE(_tama, m) {
  //===--------------------------------------------------------------------===//
  // tama dialect
  //===--------------------------------------------------------------------===//
  m.def(
      "register_dialect",
      [](MlirContext context, bool load) {
        MlirDialectHandle handle = mlirGetDialectHandle__tama__();
        mlirDialectHandleRegisterDialect(handle, context);
        if (load) {
          mlirDialectHandleLoadDialect(handle, context);
        }
      },
      "context"_a = py::none(), "load"_a = true);

  mlir_type_subclass customType =
      mlir_type_subclass(m, "CustomType", mlirTypeIsATamaCustomType);
  customType.def_classmethod(
      "get",
      [](const py::object &cls, const std::string &value, MlirContext ctx) {
        return cls(mlirTamaCustomTypeGet(
            ctx, mlirStringRefCreate(value.data(), value.size())));
      },
      "Get an instance of OperationType in given context.", "cls"_a, "value"_a,
      "context"_a = py::none());
}
