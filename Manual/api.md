# API Reference

## Python CLI

### Type Check a File

```bash
python -m Python.cli <file.pind> [options]
```

**Options:**
- `--nf <name>` — Compute normal form of a global definition
- `--strategy <strategy>` — Definitional equality strategy (`greedy`, `whnf`, `whnfv2`)
- `--verbose` — Show detailed output

**Examples:**

```bash
# Type check
python -m Python.cli Example/Nat.pind

# Compute normal form
python -m Python.cli Example/Nat.pind --nf addComm

# Use greedy strategy
python -m Python.cli Example/Nat.pind --strategy greedy
```

### Launch GUI

```bash
python -m Python.gui <file.pind>
```

Requires PySide6.

---

## Python API

### Pipeline Functions

```python
from Python.pipeline import run_pipeline, check_file, normal_form

# Run full pipeline
tokens, decls, core_decls, global_ctx = run_pipeline(source_code)

# Check a file
tokens, decls, core_decls, global_ctx = check_file("path/to/file.pind")

# Compute normal form
nf_term = normal_form(term, global_ctx)
```

### Elaboration

```python
from Python.elaborator import elaborate, elaborate_result
from Python.lexer import tokenize
from Python.parser import parse

tokens = tokenize(source)
decls = parse(tokens)

# Get core declarations directly
core_decls = elaborate(decls)

# Get both restricted and core declarations
result = elaborate_result(decls)
restricted = result.restricted_decls
core = result.core_decls
```

### Type Checking

```python
from Python.typechecker import check_program

# Check core declarations, returns global context
global_ctx = check_program(core_decls, conv_strategy="whnf")
```

### Reduction

```python
from Python.reducer import Reducer

reducer = Reducer(global_ctx, conv_strategy="whnf")

# Weak head normal form
whnf_term = reducer.whnf(term)

# Full normal form
nf_term = reducer.nf(term)

# Definitional equality
is_equal = reducer.is_def_eq(lhs, rhs)
```

### Pretty Printing

```python
from Python.pretty import show_term

print(show_term(term))
```

---

## C API

### Pipeline

```c
#include "pipeline.h"

PipelineResult result;
if (run_pipeline(source, CONV_WHN, &result)) {
    // Success: result.global_ctx contains checked definitions
    // ...
    pipeline_result_free(&result);
} else {
    // Error: result.error_msg contains the error message
    fprintf(stderr, "Error: %s\n", result.error_msg);
}
```

### Type Checker

```c
#include "typechecker.h"

GlobalContext global_ctx;
global_ctx_init(&global_ctx);

TypeChecker tc;
typechecker_init(&tc, &global_ctx, CONV_WHN);

// Register declarations
typechecker_register_definition(&tc, "name", typ, value, "var");
typechecker_register_type_ctor(&tc, ...);
typechecker_register_data_ctor(&tc, ...);
typechecker_register_eliminator(&tc, ...);

// Type check
Term* inferred = typechecker_infer(&tc, term, &ctx);
bool ok = typechecker_check(&tc, term, expected, &ctx);
```

### Reducer

```c
#include "reducer.h"

Reducer r;
reducer_init(&r, &global_ctx, CONV_WHN);

// Reduction
Term* whnf = reducer_whnf(&r, term);
Term* nf = reducer_nf(&r, term);
bool eq = reducer_is_def_eq(&r, lhs, rhs);
```

### Term Construction

```c
#include "term.h"
#include "core_ops.h"

// Create terms
Term* nat = term_new_global("Nat");
Term* zero = term_new_global("zero");
Term* succ = term_new_global("succ");
Term* succ_zero = term_new_app(succ, zero);

// De Bruijn operations
Term* shifted = term_shift(term, 1, 0);
Term* substituted = term_subst(term, 0, replacement);
Term* instantiated = term_instantiate(body, arg);
```

### Context

```c
#include "context.h"

Context ctx;
ctx_init(&ctx);

// Extend context
ctx_extend(&ctx, "x", nat_type);

// Lookup by de Bruijn index
const Term* ty = ctx_lookup(&ctx, 0);

// Cleanup
ctx_free(&ctx);
```

---

## Strategies

### `whnf` (Default)

Reduce both sides to WHNF, then structural comparison.

```
1. Check syntactic equality
2. WHNF both sides (with delta)
3. Structural compare:
   - Type == Type → true
   - Var(i) == Var(j) → i == j
   - Global(g) == Global(g') → g == g'
   - Pi(...) == Pi(...) → recurse on domain/codomain
   - Lam(...) == Lam(...) → recurse on param_type/body
   - App(...) == App(...) → unfold_app, compare head + args
```

### `greedy`

Aggressively unfold all globals, then compare.

```
1. Check syntactic equality
2. If both globals, unfold both and compare
3. If one is a global, unfold it and compare
4. If both apps, compare structurally, if fails, step both
5. Cross-type cases: try stepping the reducible side
```

### `whnfv2`

Selective unfolding by dependency height.

```
1. Check syntactic equality
2. WHNF both sides (NO delta)
3. If equal, done
4. Otherwise, pick lowest-height expandable global
5. Expand that global on both sides
6. Repeat from step 2
```

---

## Data Structures

### Core Terms (Python)

```python
@dataclass(frozen=True)
class CVar(CTerm):
    index: int
    name: Optional[str] = None

@dataclass(frozen=True)
class CGlobal(CTerm):
    name: str

@dataclass(frozen=True)
class CType(CTerm):
    pass

@dataclass(frozen=True)
class CPi(CTerm):
    name: str
    domain: CTerm
    codomain: CTerm

@dataclass(frozen=True)
class CLam(CTerm):
    name: str
    param_type: CTerm
    body: CTerm

@dataclass(frozen=True)
class CApp(CTerm):
    func: CTerm
    arg: CTerm
```

### Core Terms (C)

```c
typedef enum {
    TERM_VAR,
    TERM_GLOBAL,
    TERM_TYPE,
    TERM_PI,
    TERM_LAM,
    TERM_APP,
} TermKind;

typedef struct Term {
    TermKind kind;
    union {
        struct { uint32_t index; const char* name; } var;
        const char* global;
        struct { const char* name; const Term* domain; const Term* codomain; } pi;
        struct { const char* name; const Term* param_type; const Term* body; } lam;
        struct { const Term* func; const Term* arg; } app;
    };
};
```

### Global Context (Python)

```python
@dataclass
class TcGlobalContext:
    globals: dict[str, GlobalEntry]
    inductives: dict[str, InductiveInfo]
    constructors: dict[str, ConstructorInfo]
    eliminators: dict[str, EliminatorInfo]
```

### Global Context (C)

```c
typedef struct {
    GlobalEntry globals[MAX_GLOBALS];
    uint32_t global_count;
    
    InductiveInfo inductives[MAX_INDUCTIVES];
    uint32_t inductive_count;
    
    ConstructorInfo constructors[MAX_CONSTRUCTORS];
    uint32_t constructor_count;
    
    EliminatorInfo eliminators[MAX_ELIMINATORS];
    uint32_t eliminator_count;
} GlobalContext;
```

---

## Error Types

### Python

```python
class TinyCheckerError(Exception): ...
class LexError(TinyCheckerError): ...
class ParseError(TinyCheckerError): ...
class ElabError(TinyCheckerError): ...
class TypeCheckError(TinyCheckerError): ...
class ReducerError(TinyCheckerError): ...
```

### C

Errors are reported via:
- Return values (`bool` for success/failure)
- Error message strings in structs
- `has_error` flags

```c
typedef struct {
    bool has_error;
    char error_msg[256];
} Parser;

typedef struct {
    bool success;
    char error_msg[256];
} PipelineResult;
```
