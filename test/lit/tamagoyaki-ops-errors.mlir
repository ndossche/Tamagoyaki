// RUN: tamagoyaki-opt --verify-diagnostics %s

// ===----------------------------------------------------------------------===//
// Test tama.eq verification - error cases
// ===----------------------------------------------------------------------===//

// Test: eq operand cannot be result of another eq operation
func.func @test_eq_nested_eq() {
    %0 = arith.constant 1 : i32
    %1 = tama.eq %0 : i32
    // expected-error@+1 {{result of an eq operation cannot be used as an operand of another eq}}
    %2 = tama.eq %1 : i32
    return
}

// Test: eq operand must only be used by the eq operation
func.func @test_eq_operand_reuse() {
    %0 = arith.constant 1 : i32
    %1 = arith.constant 2 : i32
    // expected-error@+1 {{operands must only be used by the eq operation}}
    %2 = tama.eq %0 : i32
    %3 = arith.addi %0, %1 : i32
    return
}

// Test: eq operand used in multiple eq operations
func.func @test_eq_operand_multiple_users() {
    %0 = arith.constant 1 : i32
    // expected-error@+1 {{operands must only be used by the eq operation}}
    %1 = tama.eq %0 : i32
    %2 = tama.eq %0 : i32
    return
}
