// RUN: tamagoyaki-opt %s | tamagoyaki-opt | FileCheck %s

module {
    // CHECK-LABEL: func @bar()
    func.func @bar() {
        %0 = arith.constant 1 : i32
        // CHECK: %{{.*}} = equivalence.foo %{{.*}} : i32
        %res = equivalence.foo %0 : i32
        return
    }

    // CHECK-LABEL: func @equivalence_types(%arg0: !equivalence.custom<"10">)
    func.func @equivalence_types(%arg0: !equivalence.custom<"10">) {
        return
    }
}
