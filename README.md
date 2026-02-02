<p align="center" width="100%">
    <img src="tamagoyaki.png" width="200" />
</p>

# Tamagoyaki

[![Nightly Test](https://github.com/jumerckx/tamagoyaki/actions/workflows/test.yml/badge.svg)](https://github.com/jumerckx/tamagoyaki/actions/workflows/test.yml)

Tamagoyaki is an MLIR-based framework for encoding e-graphs directly in IR, and running equality saturation.
It builds on the [`pdl` dialect](https://mlir.llvm.org/docs/Dialects/PDLOps/) for rewrite pattern definitions.

## Dialects

### `tama` Dialect

The `tama` dialect provides three core operations for representing and manipulating e-graphs:

- **`tama.egraph`**: Defines an e-graph region—a single-block region containing unordered operations and equivalence classes
- **`tama.eq`**: Represents an equivalence class containing a set of equivalent values
- **`tama.yield`**: Terminator operation for `tama.egraph` regions

Example:

```mlir
func.func @main(%a: i32) -> (i32, i32) {
  %res:2 = tama.egraph %a : i32 -> i32, i32 {
  ^bb0(%b: i32):
    %b_eclass = tama.eq %b : i32
    %one = arith.constant 1 : i32
    %one_eclass = tama.eq %one : i32
    %add = arith.addi %b_eclass, %one_eclass : i32
    %add_eclass = tama.eq %add : i32
    tama.yield %one_eclass, %add_eclass : i32, i32
  }
  return %res#0, %res#1 : i32, i32
}
```

The dialect also provides the `-tama-insert-egraph` pass, which transforms a module by:
- Inserting a `tama.egraph` operation in every single-block `func.func`
- Wrapping all values and operands in `tama.eq` operations

### `ematch` Dialect

The `ematch` dialect extends the `pdl_interp` dialect to support e-matching for equality saturation. (Details are still in development.)

## Building

### Prerequisites

This project uses [mlir-wheels](https://makslevental.github.io/wheels) for MLIR distribution. Install dependencies with:

```shell
pip install -r requirements.txt
pip download mlir -f https://makslevental.github.io/wheels
unzip mlir-*.whl
```

### CMake Configuration

Configure the build with:

```shell
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE=$(which python) \
  -DCMAKE_PREFIX_PATH=$PWD/mlir \
  -DLLVM_EXTERNAL_LIT=$(which lit) \
  -B build \
  -S $PWD
```

### Running Tests

Build and run the test suite:

```shell
cd build
ninja check-tamagoyaki
```

## About

This project's build configuration is based on [Max Levental](https://makslevental.github.io/about/)'s [mmlir](https://github.com/makslevental/mmlir) example repository.
