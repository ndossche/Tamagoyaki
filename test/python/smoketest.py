# RUN: %python %s | FileCheck %s

from mmlir.dialects import potato as potato_d
from mmlir.ir import *

with Context():
    potato_d.register_dialect()
    module = Module.parse(
        """
    %0 = arith.constant 2 : i32
    %1 = potato.foo %0 : i32
    """
    )
    # CHECK: %[[C:.*]] = arith.constant 2 : i32
    # CHECK: potato.foo %[[C]] : i32
    print(str(module))

    t = potato_d.CustomType.get("hello")
    # CHECK: !potato.custom<"hello">
    print(t)
