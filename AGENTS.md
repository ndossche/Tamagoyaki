# AGENTS.md: Tamagoyaki MLIR Dialect

A guide to developing MLIR dialects for equivalence graph (e-graph) operations in this project.

This document covers:
- **Tama dialect**: E-graph operations (`tama.eq`, `tama.egraph`, `tama.yield`)
- **Tamatch dialect**: Pattern matching operations (example dialect)

## Operations Reference

### tama.foo

**Purpose**: Illustrative operation demonstrating operation definition.

**Traits**: `Pure`, `SameOperandsAndResultType`

**Signature**:
```mlir
%result = tama.foo %input : i32
```

**Definition** (`include/TamagoyakiDialect.td:50`):
- Takes single `i32` input operand
- Returns `i32` result of same type
- Pure operation (no side effects)
- Assembly format: `%input `: ` type`

**Usage**: Test/example purposes only; used to validate dialect infrastructure.

---

### tama.eq

**Purpose**: Represents an equivalence class containing a set of equivalent values. Operations marked with `eq` must satisfy strict verification constraints.

**Traits**: `Pure`, `SameOperandsAndResultType`

**Signature**:
```mlir
%eq_result = tama.eq %value1, %value2, ... : type
%eq_result = tama.eq %value1 {min_cost_index = 0 : i64} : type
```

**Arguments**:
- `inputs` (Variadic<AnyType>): The values in this equivalence class
- `min_cost_index` (OptionalAttr<I64Attr>): Optional index of minimum-cost value

**Results**: One result of same type as operands

**Assembly Format**: `inputs attr-dict `:` type(result)`

**Verification Rules** (`src/TamagoyakiDialect.cpp:48`):

1. **Non-empty**: Must have at least one operand
   ```
   Error: "must have at least one operand"
   ```

2. **No nested eq operations**: Operands cannot be results of other `eq` operations
   ```
   Error: "result of an eq operation cannot be used as an operand of another eq"
   ```

3. **Exclusive use**: Operands must only be used by this `eq` operation
   ```
   Error: "operands must only be used by the eq operation"
   ```

**Example - Valid**:
```mlir
%0 = arith.constant 1 : i32
%1 = tama.eq %0 : i32
```

**Example - Invalid (nested eq)**:
```mlir
%0 = arith.constant 1 : i32
%1 = tama.eq %0 : i32
// ERROR: Cannot use %1 (eq result) as operand to another eq
%2 = tama.eq %1 : i32
```

**Example - Invalid (operand reuse)**:
```mlir
%0 = arith.constant 1 : i32
%1 = tama.eq %0 : i32
// ERROR: %0 used by arith.addi, violates exclusive-use rule
%2 = arith.addi %0, %0 : i32
```

---

### tama.egraph

**Purpose**: Defines an e-graph region—a single-block region containing unordered operations and equivalence classes.

**Traits**: 
- `SingleBlockImplicitTerminator<"YieldOp">` — ensures one block ending with `tama.yield`
- `IsolatedFromAbove` — isolates region from surrounding context

**Signature**:
```mlir
// No inputs
%results = tama.egraph -> type1, type2 {
  ^bb0:
    // operations
    tama.yield %value1, %value2 : type1, type2
}

// With inputs
%results = tama.egraph %input : type1 -> type2 {
  ^bb0(%arg0: type1):
    // operations
    tama.yield %result : type2
}
```

**Arguments**: `inputs` (Variadic<AnyType>) — optional input values

**Results**: `outputs` (Variadic<AnyType>) — output values of specified types

**Regions**: `body` (SizedRegion<1>) — exactly one block

**Assembly Format**: `($inputs^ `:` type($inputs))? `->` type($outputs) $body attr-dict`

**Properties**:
- Operations inside are unordered (use-def can appear before definition)
- Block arguments accessible to all operations in the block
- Terminator must be `tamagoyaki.yield`

**Example**:
```mlir
func.func @main(%arg0: i32) -> (i32, i32) {
  %0:2 = tama.egraph %arg0 : i32 -> i32, i32 {
  ^bb0(%arg1: i32):
    %c1_i32 = arith.constant 1 : i32
    %1 = arith.addi %arg1, %c1_i32 : i32
    tama.yield %c1_i32, %1 : i32, i32
  }
  return %0#0, %0#1 : i32, i32
}
```

---

### tama.yield

**Purpose**: Terminator operation for `tama.egraph` regions.

**Traits**: 
- `Terminator` — marks as block terminator
- `HasParent<"EGraphOp">` — ensures it only appears in egraph regions

**Signature**:
```mlir
tama.yield %value1, %value2 : type1, type2
tama.yield %value : type
tama.yield : // empty yield
```

**Arguments**: `values` (Variadic<AnyType>) — values yielded from egraph

**Assembly Format**: `$values `:` type($values) attr-dict`

**Builders**: Default builder with no arguments (auto-invoked by `SingleBlockImplicitTerminator`)

---

### tama.custom<"value"> Type

**Purpose**: Custom parametric type for the dialect.

**Syntax**: `!tama.custom<"string_value">`

**Parameter**: String-valued custom data

**Example**:
```mlir
func.func @tama_types(%arg0: !tama.custom<"10">) {
  return
}
```

---

### tamatch.foo

**Purpose**: Illustrative operation demonstrating operation definition in the Tamatch dialect.

**Traits**: `Pure`, `SameOperandsAndResultType`

**Signature**:
```mlir
%result = tamatch.foo %input : i32
```

**Definition** (`include/TamatchDialect.td:45`):
- Takes single `i32` input operand
- Returns `i32` result of same type
- Pure operation (no side effects)
- Assembly format: `%input `: ` type`

**Usage**: Test/example purposes; demonstrates dialect infrastructure and multi-dialect support.

**Example**:
```mlir
func.func @example() {
  %0 = arith.constant 42 : i32
  %1 = tamatch.foo %0 : i32
  return
}
```

---

## Transformation Passes

### tama-insert-egraph

**Command**: `-tama-insert-egraph`

**Target**: `::mlir::ModuleOp`

**Dependent Dialects**: `TamaDialect`, `func::FuncDialect`

**Summary**: Wraps function bodies in `tama.egraph` operations and wraps all values/operands in `tama.eq` operations.

**When to Use**:
- Preparing IR for e-graph based transformations
- Must be applied before passes that depend on egraph structure

**Transformation Behavior**:

1. **Collect functions**: Iterates module-level operations
2. **Transform each single-block function**:
    - Creates `tama.egraph` operation with function result types
    - Moves function body into egraph region
    - Replaces `func.return` with `tama.yield`
    - Wraps all values in `tama.eq` operations (recursive, depth-first)
    - Creates new entry block with function arguments
    - Returns egraph results via new `func.return`

**Value Wrapping Logic** (`src/TamagoyakiDialect.cpp:84`):

- Wraps block arguments first (at block start)
- Processes operations in reverse order to avoid iterator invalidation
- Recursively processes nested regions
- Skips existing `tama.eq` operations (prevents nesting violations)
- For each value, creates `tama.eq` and replaces uses (except the eq itself)

**Example Transformation**:

**Input**:
```mlir
func.func @main(%arg0: i32) -> (i32, i32) {
  %a = arith.constant 1 : i32
  %b = arith.addi %arg0, %a : i32
  return %a, %b : i32, i32
}
```

**Output**:
```mlir
func.func @main(%arg0: i32) -> (i32, i32) {
  %0:2 = tama.egraph %arg0 : i32 -> i32, i32 {
  ^bb0(%arg1: i32):
    %1 = tama.eq %arg1 : i32
    %c1_i32 = arith.constant 1 : i32
    %2 = tama.eq %c1_i32 : i32
    %3 = arith.addi %1, %2 : i32
    %4 = tama.eq %3 : i32
    tama.yield %2, %4 : i32, i32
  }
  return %0#0, %0#1 : i32, i32
}
```

**Failure Cases**:
- Multi-block functions (returns failure, no transformation)
- Functions without `func.return` terminator (emits error)

---

### tama-switch-bar-foo

**Command**: `-tama-switch-bar-foo` or `--pass-pipeline="builtin.module(tama-switch-bar-foo)"`

**Target**: `::mlir::ModuleOp`

**Summary**: Renames any `func.func` named `"bar"` to `"foo"` using pattern matching and greedy rewrite.

**When to Use**:
- Example pass showing pattern matching and in-place modification
- Testing pass pipeline infrastructure

**Rewrite Pattern** (`src/TamagoyakiDialect.cpp:209`):

- Matches: `func::FuncOp` with symbol name `"bar"`
- Action: Modifies operation in-place: `op.setSymName("foo")`
- Result: Returns `success()` for matched operations

**Example Transformation**:

**Input**:
```mlir
module {
  func.func @bar() {
    return
  }
  func.func @abar() {
    return
  }
}
```

**Output**:
```mlir
module {
  func.func @foo() {
    return
  }
  func.func @abar() {
    return
  }
}
```

---

## Writing Tests

### Test Location and Format

**Path**: `test/lit/*.mlir`

**Format**: MLIR + Lit directives (RUN and CHECK)

**Test Runner**: LLVM's `lit` (Lit Infrastructure Test)

### Lit Test Directives

| Directive | Purpose | Example |
|-----------|---------|---------|
| `// RUN:` | Command to execute | `// RUN: tamagoyaki-opt %s \| FileCheck %s` |
| `// CHECK:` | Exact line match | `// CHECK: tamagoyaki.foo` |
| `// CHECK-LABEL:` | Label anchor for CHECK blocks | `// CHECK-LABEL: func @bar()` |
| `// CHECK-NEXT:` | Must appear on next line | `// CHECK-NEXT: return` |
| `// expected-error@+N:` | Expect error on line N | `// expected-error@+1` |
| `// XFAIL:` | Mark test as expected to fail | `// XFAIL: *` |

### Common Patterns

**Pattern 1: Operation Parsing and Printing**

Test that operations parse and print correctly:

```mlir
// RUN: tamagoyaki-opt %s | tamagoyaki-opt | FileCheck %s

module {
  // CHECK-LABEL: func @bar()
  func.func @bar() {
    %0 = arith.constant 1 : i32
    // CHECK: tamagoyaki.foo %{{.*}} : i32
    %res = tamagoyaki.foo %0 : i32
    return
  }
}
```

- `%{{.*}}` matches any SSA value
- Double-piped through `tamagoyaki-opt` to verify round-trip

**Pattern 2: Pass Transformation**

Test that a pass produces expected output:

```mlir
// RUN: tamagoyaki-opt -pass-name %s | FileCheck %s

// CHECK:      func.func @main(%arg0: i32) -> i32 {
// CHECK-NEXT:   %0 = tamagoyaki.egraph %arg0 : i32 -> i32 {
// CHECK-NEXT:   ^bb0(%arg1: i32):
// ...
// CHECK-NEXT:   }
// CHECK-NEXT:   return %0 : i32
// CHECK-NEXT: }

func.func @main(%arg0: i32) -> i32 {
  return %arg0 : i32
}
```

**Pattern 3: Verification Error Testing**

Test that invalid IR is caught:

```mlir
// RUN: tamagoyaki-opt --verify-diagnostics %s

func.func @invalid_eq() {
  %0 = arith.constant 1 : i32
  // expected-error@+1 {{must have at least one operand}}
  %1 = tamagoyaki.eq : i32
  return
}
```

- `--verify-diagnostics` mode captures and verifies expected error messages
- `// expected-error@+N` marks the line N lines below as error site
- Regex pattern in `{{...}}` matches error message

**Pattern 4: Custom Types**

Test custom type parsing and round-trip:

```mlir
// RUN: tamagoyaki-opt %s | FileCheck %s

// CHECK-LABEL: func @tamagoyaki_types(%arg0: !tamagoyaki.custom<"10">)
func.func @tamagoyaki_types(%arg0: !tamagoyaki.custom<"10">) {
  return
}
```

### Test File Organization

| File | Purpose |
|------|---------|
| `test/lit/smoke.mlir` | Basic operation parsing and printing |
| `test/lit/pass.mlir` | tamagoyaki-switch-bar-foo pass |
| `test/lit/tamagoyaki-insert-egraph.mlir` | tamagoyaki-insert-egraph pass |
| `test/lit/tamagoyaki-ops-errors.mlir` | Operation verification errors |

### Naming Convention

- `*-errors.mlir` — verification and error testing
- `*-insert-egraph.mlir` — insert-egraph pass behavior
- `smoke.mlir` — basic functionality checks
- `pass.mlir` — generic pass testing

---

## Building & Development

### Build Commands

**Configure with CMake** (one-time):
```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPython3_EXECUTABLE=$(which python) \
  -DCMAKE_PREFIX_PATH=$PWD/mlir \
  -DLLVM_EXTERNAL_LIT=$(which lit) \
  -B build \
  -S $PWD
```

**Build**:
```bash
cd build
ninja
```

**Build specific target**:
```bash
ninja tamagoyaki-opt
```

**Rebuild after changes**:
```bash
ninja  # incremental
ninja -j1  # single-threaded (for debugging)
```

### Running Tests

**All tests**:
```bash
cd build
ninja test
```

**Lit tests only**:
```bash
cd build
ninja check-tamagoyaki
```

**Single test file**:
```bash
cd build
lit ../test/lit/smoke.mlir -v
```

**With debug output**:
```bash
cd build
lit ../test/lit/tamagoyaki-insert-egraph.mlir -v -a
```

### tamagoyaki-opt Tool

**Basic usage**:
```bash
./build/bin/tamagoyaki-opt input.mlir
```

**With pass**:
```bash
./build/bin/tamagoyaki-opt -tama-insert-egraph input.mlir
```

**With pass pipeline**:
```bash
./build/bin/tamagoyaki-opt --pass-pipeline="builtin.module(tama-switch-bar-foo)" input.mlir
```

**Print generic form** (debug):
```bash
./build/bin/tamagoyaki-opt input.mlir -mlir-print-op-generic
```

**Verify IR**:
```bash
./build/bin/tamagoyaki-opt input.mlir -verify-diagnostics
```

**Apply multiple passes**:
```bash
./build/bin/tamagoyaki-opt \
  --pass-pipeline="builtin.module(tama-insert-egraph,tama-switch-bar-foo)" \
  input.mlir
```

### Python Bindings Workflow

**Python module structure**:
```
python/
  mmlir/
    dialects/
      tamagoyaki.py          # imports generated bindings
  Tamagoyaki.td              # Python ODS definitions
  TamagoyakiExtension.cpp    # C++ extension code
```

**Building with Python bindings** (requires Python dev environment):
```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMLIR_ENABLE_BINDINGS_PYTHON=ON \
  -DPYTHON_EXECUTABLE=$(which python3) \
  ..
ninja
```

**Using Python bindings** (after build):
```python
from mlir import ir
from mmlir.dialects import tamagoyaki

# Create context and load dialect
ctx = ir.Context()
ctx.load_dialect(tamagoyaki.Tamagoyaki)

# Parse MLIR with tamagoyaki dialect
module = ir.Module.parse("""
  func.func @main() {
    return
  }
""")
```

---

## Common Development Tasks

### Adding a New Operation

1. **Define in TableGen** (`include/TamagoyakiDialect.td`):
   ```tablegen
   def Tamagoyaki_NewOp : Tamagoyaki_Op<"newop", [Pure]> {
       let summary = "Description of new operation";
       let description = [{
           Longer description and examples.
       }];
       
       let arguments = (ins I32:$input);
       let results = (outs I32:$result);
       
       let assemblyFormat = [{
           $input attr-dict `:` type($input)
       }];
   }
   ```

2. **Add verification** (if needed) in `src/TamagoyakiDialect.cpp`:
   ```cpp
   LogicalResult NewOp::verify() {
       // Verification logic
       return success();
   }
   ```

3. **Rebuild**:
   ```bash
   cd build && ninja
   ```

4. **Test** with Lit test in `test/lit/`:
   ```mlir
   // RUN: tamagoyaki-opt %s | FileCheck %s
   
   // CHECK: tamagoyaki.newop
   %0 = tamagoyaki.newop %input : i32
   ```

### Adding a New Pass

1. **Define in TableGen** (`include/TamagoyakiDialect.td`):
   ```tablegen
   def TamagoyakiMyPass : Pass<"tamagoyaki-my-pass", "::mlir::ModuleOp"> {
       let summary = "Short description";
       let dependentDialects = ["::mlir::tama::TamaDialect"];
   }
   ```

2. **Implement in** `src/TamagoyakiDialect.cpp`:
   ```cpp
   namespace {
   
   class TamagoyakiMyPass
       : public impl::TamagoyakiMyPassBase<TamagoyakiMyPass> {
   public:
       using impl::TamagoyakiMyPassBase<
           TamagoyakiMyPass>::TamagoyakiMyPassBase;
       void runOnOperation() final {
           ModuleOp module = getOperation();
           // Transform logic
       }
   };
   
   } // namespace
   ```

3. **Register in dialect initialization** (`src/TamagoyakiDialect.cpp`):
   - Passes auto-register via generated code in headers
   - Ensure `#define GEN_PASS_DEF_TAMAGOYAKIMYPASS` is present

4. **Test with Lit**:
   ```bash
   ./build/bin/tamagoyaki-opt -tamagoyaki-my-pass input.mlir
   ```

### Modifying Existing Operations

1. **Update TableGen** (`include/TamagoyakiDialect.td`)
2. **Update/add verification** in `src/TamagoyakiDialect.cpp` if constraints change
3. **Rebuild**:
   ```bash
   cd build && ninja
   ```
4. **Update/add tests** in `test/lit/`

### Pattern Matching & Rewrite Patterns

**Reference Implementation** (`src/TamagoyakiDialect.cpp:209`):

```cpp
class TamagoyakiRewritePattern : public OpRewritePattern<TargetOpType> {
public:
  using OpRewritePattern<TargetOpType>::OpRewritePattern;
  
  LogicalResult matchAndRewrite(TargetOpType op,
                                PatternRewriter &rewriter) const final {
    // Matching logic
    if (!isInteresting(op)) 
      return failure();
    
    // Rewriting logic
    rewriter.modifyOpInPlace(op, [&op]() { 
      // Modify op in-place
    });
    
    return success();
  }
};
```

**Applying patterns**:

```cpp
RewritePatternSet patterns(&context);
patterns.add<YourPattern>(&context);
FrozenRewritePatternSet frozen(std::move(patterns));
applyPatternsGreedily(operation, frozen);
```

---

## Debugging

### Inspecting IR Structure

**Print generic (canonical) form**:
```bash
./build/bin/tamagoyaki-opt -mlir-print-op-generic input.mlir
```

Shows SSA values, types, and attributes in unambiguous format:
```mlir
"func.func"() ({
  %0 = "tamagoyaki.foo"(%arg0) : (i32) -> i32
  "func.return"(%0) : (i32) -> ()
}) {sym_name = "test", type = (i32) -> i32} : () -> ()
```

**Print with attributes**:
```bash
./build/bin/tamagoyaki-opt -print-op-on-diagnostic input.mlir
```

### Enabling Debug Output

**Build with assertions and debug info**:
```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

**Run with debug flags in passes**:

Add logging to pass code:
```cpp
LLVM_DEBUG(llvm::dbgs() << "Processing operation: " << op << "\n");
```

Enable debug output:
```bash
MLIR_DEBUG=1 ./build/bin/tamagoyaki-opt input.mlir 2>&1 | grep "Processing"
```

**Use mlir-opt debugging**:
```bash
./build/bin/tamagoyaki-opt input.mlir -debug -debug-pass=all 2>&1 | head -100
```

### Common Verification Errors & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `must have at least one operand` | Empty `tama.eq` | Add operand to eq |
| `result of an eq operation cannot be used as an operand of another eq` | Nested eq operations | Flatten: use original operands |
| `operands must only be used by the eq operation` | Value used elsewhere | Wrap uses in eq, or restructure |
| `expected block with 1 block but got N` | egraph has multiple blocks | egraph must be single-block; use `SingleBlockImplicitTerminator` |
| `expected terminator "tama.yield"` | egraph lacks yield | Add `tama.yield` before block end |
| `HasParent constraint violated` | yield outside egraph | Move yield inside egraph or use within egraph only |

**Verify IR explicitly**:
```bash
./build/bin/tamagoyaki-opt --verify-diagnostics input.mlir
```

Stops on first error; useful for catching issues early.

---

## Directory Structure

```
tamagoyaki/
├── include/
│   ├── TamagoyakiDialect.td         # TableGen definitions (ops, passes, types)
│   ├── TamagoyakiDialect.h          # Header (includes generated code)
│   └── *.h.inc, *.cpp.inc           # Generated from TableGen
├── src/
│   ├── TamagoyakiDialect.cpp        # Operation/pass implementations
│   ├── tamagoyaki-opt.cpp           # Optimizer driver
│   └── TamagoyakiCAPI.cpp           # C API bindings
├── test/
│   ├── lit/
│   │   ├── smoke.mlir               # Basic tests
│   │   ├── pass.mlir                # Pass tests
│   │   ├── tamagoyaki-insert-egraph.mlir
│   │   └── tamagoyaki-ops-errors.mlir
│   ├── lit.cfg.py                   # Lit configuration
│   └── CMakeLists.txt
├── python/
│   ├── mmlir/dialects/tamagoyaki.py # Python bindings
│   └── TamagoyakiExtension.cpp      # Extension code
├── CMakeLists.txt                   # Root CMake config
└── build/                           # (generated) Build output
```

---

## References

- **MLIR Dialect Tutorial**: https://mlir.llvm.org/docs/Tutorials/Toy/Ch-2/
- **TableGen Backend Reference**: https://llvm.org/docs/TableGen/
- **MLIR Passes**: https://mlir.llvm.org/docs/Passes/
- **Lit Testing**: https://llvm.org/docs/CommandGuide/lit/
