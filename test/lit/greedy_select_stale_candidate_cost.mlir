// RUN: tamagoyaki-opt "--equivalence-select-greedy=default-cost=1" %s | FileCheck %s

// CHECK:      %[[Y:.*]] = equivalence.class %{{.*}}, %{{.*}} (min_cost_index = 1)
// CHECK:      %[[C:.*]] = arith.addi %[[Y]], %arg0
// CHECK:      equivalence.class %[[C]], %{{.*}} (min_cost_index = 0)

func.func @main(%arg0: i32) -> i32 {
  %0 = equivalence.graph -> (i32) {
    %a_direct = arith.addi %arg0, %arg0 {equivalence.cost = #equivalence.cost<10>} : i32
    %a_forward = arith.addi %z, %arg0 : i32
    %y = equivalence.class %a_direct, %a_forward : i32

    // Forward referenced from %a_forward such that the cost iterator has to run for multiple iterations
    %z = arith.constant 1 : i32

    %c = arith.addi %y, %arg0 : i32
    %d = arith.addi %arg0, %arg0 {equivalence.cost = #equivalence.cost<5>} : i32

    %x = equivalence.class %c, %d : i32
    equivalence.yield %x : i32
  }
  return %0 : i32
}
