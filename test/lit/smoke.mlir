// RUN: potato-opt %s | potato-opt | FileCheck %s

module {
    // CHECK-LABEL: func @bar()
    func.func @bar() {
        %0 = arith.constant 1 : i32
        // CHECK: %{{.*}} = potato.foo %{{.*}} : i32
        %res = potato.foo %0 : i32
        return
    }

    // CHECK-LABEL: func @potato_types(%arg0: !potato.custom<"10">)
    func.func @potato_types(%arg0: !potato.custom<"10">) {
        return
    }
}
