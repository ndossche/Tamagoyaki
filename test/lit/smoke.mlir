// RUN: tamagoyaki-opt %s | tamagoyaki-opt | FileCheck %s

module {
    // CHECK-LABEL: func @bar()
    func.func @bar() {
        %0 = arith.constant 1 : i32
        // CHECK: %{{.*}} = tama.foo %{{.*}} : i32
        %res = tama.foo %0 : i32
        return
    }

    // CHECK-LABEL: func @tama_types(%arg0: !tama.custom<"10">)
    func.func @tama_types(%arg0: !tama.custom<"10">) {
        return
    }
}
