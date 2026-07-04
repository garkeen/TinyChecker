# Error Reference

## Error Hierarchy

```
TinyCheckerError
├── LexError          — Lexical analysis errors
├── ParseError        — Syntax parsing errors
├── ElabError         — Elaboration errors
├── TypeCheckError    — Type checking errors
└── ReducerError      — Reduction errors
```

---

## LexError

Errors during tokenization.

### Common Causes

| Error | Cause | Example |
|-------|-------|---------|
| `unexpected character` | Invalid character in source | `@`, `$` |
| `unmatched ')'"` | Closing bracket without opening | `)` without `(` |
| `unbalanced delimiters` | Mismatched brackets at EOF | `(` without `)` |
| `invalid dot identifier` | Dot not followed by letter | `A.1` |

### Examples

```pind
# Error: unexpected character '@'
var x: Nat = @zero;

# Error: unmatched ')'
var x: Nat = zero);

# Error: unbalanced delimiters
fun f (x: Nat): Nat {
  (succ x
# Missing closing )
```

---

## ParseError

Errors during syntax parsing.

### Common Causes

| Error | Cause | Example |
|-------|-------|---------|
| `expected X, got Y` | Wrong token type | Missing `:` in declaration |
| `unexpected token` | Token in wrong position | `}` in expression |
| `match family head must be an atom` | Non-atom after `in` | `match x in (f y) ...` |
| `equation name mismatch` | Names don't match | `addComm: T, otherName ...` |
| `'_' is not allowed as an expression` | Underscore as expression | `\_ => x` |

### Examples

```pind
# Error: expected COLON, got EQ
var x Nat = zero;

# Error: match family head must be an atom
match n in (Nat -> Type) return Type with ... end

# Error: equation name mismatch
addComm: Nat, otherName = zero;
```

---

## ElabError

Errors during elaboration (surface → core).

### Common Causes

| Error | Cause | Example |
|-------|-------|---------|
| `unbound name` | Unknown identifier | Using undefined variable |
| `duplicate global name` | Name already defined | Defining `Nat` twice |
| `constructor must return T` | Wrong return type | Constructor returning wrong type |
| `wrong family arity` | Wrong number of type args | `List Nat zero extra` |
| `must use uniform parameters` | Parameters not uniform | Constructor with changed params |
| `sum cannot have parameters` | Sum with params | `sum Bool (A: Type) { ... }` |
| `case can only eliminate sum/product` | Case on inductive | `case n in Nat ...` |
| `match can only eliminate inductive` | Match on sum | `match b in Bool ...` |
| `branches must be complete` | Missing constructor | Missing `succ` branch |
| `expected branch X, got Y` | Wrong branch order | Wrong constructor name |
| `recursive field cannot mention T` | Negative occurrence | `sum` with recursive field |

### Examples

```pind
# Error: unbound name 'z'
var x: Nat = z;

# Error: duplicate global name
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive Nat { | nothing: Nat };

# Error: case can only eliminate sum/product types
case n in Nat return Nat with
| zero => zero
| succ k => k
end

# Error: sum declarations cannot have parameters
sum Bool (A: Type) {
  | true: Bool
  | false: Bool
};
```

---

## TypeCheckError

Errors during type checking.

### Common Causes

| Error | Cause | Example |
|-------|-------|---------|
| `unbound de Bruijn variable` | Invalid index | Internal error |
| `unbound global variable` | Unknown global | Reference to undefined |
| `non-function application` | Applying non-function | `zero x` |
| `lambda expected Pi type` | Lambda against non-Pi | `(\x => x) : Nat` |
| `type mismatch` | Types don't match | `zero : Bool` |

### Examples

```pind
# Error: non-function application
var bad: Nat = zero zero;

# Error: lambda expected Pi type
var bad: Nat = \(x: Nat) => x;

# Error: type mismatch
var bad: Bool = zero;
```

### Type Mismatch Details

When a type mismatch occurs, the error shows both WHNF forms:

```
type mismatch: Nat vs Bool
```

This means after reducing both sides to WHNF, they are structurally different.

---

## ReducerError

Errors during reduction.

### Common Causes

| Error | Cause |
|-------|-------|
| `WHNF exceeded step limit` | Infinite loop or very complex reduction |
| `greedy conversion exceeded step limit` | Greedy strategy too aggressive |
| `whnfv2 conversion exceeded step limit` | whnfv2 stuck in loop |
| `unknown global` | Reference to unregistered global |

### Step Limits

Default step limit: **200,000**

Can be adjusted in Python:

```python
from Python.reducer import Reducer

reducer = Reducer(global_ctx, max_steps=500_000)
```

---

## Debugging Tips

### 1. Use `--verbose` flag

```bash
python -m Python.cli Example/Nat.pind --verbose
```

### 2. Compute normal forms

```bash
python -m Python.cli Example/Nat.pind --nf someDefinition
```

### 3. Check intermediate stages

```python
from Python.pipeline import run_pipeline
from Python.pretty import show_term

tokens, decls, core_decls, global_ctx = run_pipeline(source)

# Print core declarations
for decl in core_decls:
    print(decl)
```

### 4. Use the GUI

```bash
python -m Python.gui Example/Nat.pind
```

The GUI shows:
- Syntax-highlighted source
- Type information on hover
- Proof tree visualization

### 5. Common Patterns

**Pattern: Forgetting to provide all arguments**

```pind
# Wrong: add expects 2 arguments
var x: Nat = add zero;

# Right
var x: Nat = add zero zero;
```

**Pattern: Wrong motive in match**

```pind
# Wrong: motive doesn't match return type
match n in Nat return Bool with
| zero => zero        # zero is Nat, not Bool
| succ k => k         # k is Nat, not Bool
end

# Right
match n in Nat return Nat with
| zero => zero
| succ k => succ k
end
```

**Pattern: Missing induction hypothesis**

```pind
# Wrong: need IH for recursive case
match n in Nat return Nat with
| zero => zero
| succ k => succ k    # Can't use k recursively
end

# Right
match n in Nat return Nat with
| zero => zero
| succ k [ih] => succ ih    # ih is the recursive result
end
```

**Pattern: Using case instead of match**

```pind
# Wrong: case doesn't provide IH
case n in Nat return Nat with
| zero => zero
| succ k [ih] => ih    # Error: case doesn't have IH
end

# Right: use match for inductive types
match n in Nat return Nat with
| zero => zero
| succ k [ih] => ih
end
```

---

## Error Recovery

### Python

Errors are raised as exceptions and can be caught:

```python
from Python.errors import TypeCheckError

try:
    global_ctx = check_program(core_decls)
except TypeCheckError as e:
    print(f"Type error: {e}")
```

### C

Errors are reported via return values and error messages:

```c
PipelineResult result;
if (!run_pipeline(source, CONV_WHN, &result)) {
    printf("Error: %s\n", result.error_msg);
    pipeline_result_free(&result);
}
```
