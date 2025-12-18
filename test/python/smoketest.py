# RUN: %python %s | FileCheck %s

from mmlir.dialects import tamagoyaki as tamagoyaki_d
from mmlir.ir import *

with Context():
    tamagoyaki_d.register_dialect()
    module = Module.parse(
        """
    %0 = arith.constant 2 : i32
    %1 = tamagoyaki.foo %0 : i32
    """
    )
    # CHECK: %[[C:.*]] = arith.constant 2 : i32
    # CHECK: tamagoyaki.foo %[[C]] : i32
    print(str(module))

    t = tamagoyaki_d.CustomType.get("hello")
    # CHECK: !tamagoyaki.custom<"hello">
    print(t)
