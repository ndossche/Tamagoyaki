// RUN: tamagoyaki-opt %s | tamagoyaki-opt | FileCheck %s

module {
    // CHECK-LABEL: func @bar()
    func.func @bar() {
        %0 = arith.constant 1 : i32
        // CHECK: %{{.*}} = tamagoyaki.foo %{{.*}} : i32
        %res = tamagoyaki.foo %0 : i32
        return
    }

    // CHECK-LABEL: func @tamagoyaki_types(%arg0: !tamagoyaki.custom<"10">)
    func.func @tamagoyaki_types(%arg0: !tamagoyaki.custom<"10">) {
        return
    }
}
