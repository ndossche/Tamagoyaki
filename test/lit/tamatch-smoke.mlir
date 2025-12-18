// RUN: tamagoyaki-opt %s | FileCheck %s

module {
  // CHECK-LABEL: func @tamatch_foo_test()
  func.func @tamatch_foo_test() {
    %0 = arith.constant 2 : i32
    // CHECK: tamatch.foo %{{.*}} : i32
    %1 = tamatch.foo %0 : i32
    return
  }
}
