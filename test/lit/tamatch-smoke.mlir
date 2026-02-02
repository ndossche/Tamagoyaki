// RUN: tamagoyaki-opt %s | FileCheck %s

module {
  // CHECK-LABEL: func @ematch_foo_test()
  func.func @ematch_foo_test() {
    %0 = arith.constant 2 : i32
    // CHECK: ematch.foo %{{.*}} : i32
    %1 = ematch.foo %0 : i32
    return
  }
}
