# RUN: %python %s | FileCheck %s

from mmlir.dialects import tama as tama_d
from mmlir.ir import *

with Context():
    tama_d.register_dialect()
    module = Module.parse(
        """
    %0 = arith.constant 2 : i32
    %1 = tama.foo %0 : i32
    """
    )
    # CHECK: %[[C:.*]] = arith.constant 2 : i32
    # CHECK: tama.foo %[[C]] : i32
    print(str(module))

    t = tama_d.CustomType.get("hello")
    # CHECK: !tama.custom<"hello">
    print(t)
