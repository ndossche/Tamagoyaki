// RUN: tamagoyaki-opt -tama-insert-egraph %s | FileCheck %s

// CHECK:      func.func @main(%arg0: i32) -> (i32, i32) {
// CHECK-NEXT:   %0:2 = tama.egraph %arg0 : i32 -> i32, i32 {
// CHECK-NEXT:   ^bb0(%arg1: i32):
// CHECK-NEXT:     %1 = tama.eq %arg1 : i32
// CHECK-NEXT:     %c1_i32 = arith.constant 1 : i32
// CHECK-NEXT:     %2 = tama.eq %c1_i32 : i32
// CHECK-NEXT:     %3 = arith.addi %1, %2 : i32
// CHECK-NEXT:     %4 = tama.eq %3 : i32
// CHECK-NEXT:     tama.yield %2, %4 : i32, i32
// CHECK-NEXT:   }
// CHECK-NEXT:   return %0#0, %0#1 : i32, i32
// CHECK-NEXT: }

func.func @main(%arg0: i32) -> (i32, i32) {
  %a = arith.constant 1 : i32
  %b = arith.addi %arg0, %a : i32
  return %a, %b : i32, i32
}
