//===- tamagoyaki-opt.cpp ------------------------------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TamagoyakiDialect.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

using namespace mlir;
using namespace mlir::tamagoyaki;

int main(int argc, char **argv) {
  mlir::registerAllPasses();
  mlir::tamagoyaki::registerPasses();
  mlir::DialectRegistry registry;
  registry.insert<mlir::tamagoyaki::TamagoyakiDialect, mlir::arith::ArithDialect,
                  mlir::func::FuncDialect>();
  registerAllDialects(registry);

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "Tamagoyaki optimizer driver\n", registry));
}
