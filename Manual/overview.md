# TinyChecker Overview

## What is TinyChecker?

TinyChecker is a **dependently-typed language type checker and proof assistant** for the **Pind** (Pi + Inductive) language. It provides:

- A formally specified type system with dependent function types (Pi types)
- Inductive types with parameters and indices
- Sum types (tagged unions) and product types (records)
- Pattern matching with automatic induction hypothesis generation
- Propositional equality and proof construction
- Multiple definitional equality strategies
- Both Python and C implementations with identical semantics

## Language Name

**Pind** = **Pi** (dependent function types) + **Ind**uctive types

## Core Concepts

### 1. Dependent Types

Types can depend on values:

```pind
# A function that takes a natural number and returns a type
Vec : (A: Type) -> Nat -> Type

# The return type depends on the input value
length : (A: Type) -> (n: Nat) -> Vec A n -> Nat
```

### 2. Propositions as Types

Propositions are represented as types. A proof of a proposition is a term of that type:

```pind
# The proposition "n = n" is represented as:
[Nat]n==n

# A proof:
refl Nat n : [Nat]n==n
```

### 3. Induction via Eliminators

All recursion goes through eliminators (pattern matching). Direct recursion is not allowed:

```pind
# Correct: using match/eliminator
fun add (m: Nat) (n: Nat): Nat {
  (match m in Nat
   return Nat -> Nat with
   | zero => \(n: Nat) => n
   | succ k [rec] => \(n: Nat) => succ (rec n)
   end)
  n
};
```

### 4. Induction Hypotheses

The `[ih]` syntax in match branches binds induction hypotheses for recursive fields:

```pind
match n as k in Nat
return [Nat](add k zero)==k with
| zero => refl Nat zero              # base case: no IH needed
| succ k [ih] => ...                 # ih : [Nat](add k zero)==k
end
```

## Supported Features

| Feature | Description |
|---------|-------------|
| Pi Types | `(x: A) -> B` - dependent function types |
| Lambda | `\(x: A) => body` - function abstraction |
| Application | `f x y` - function application |
| Inductive Types | Parameterized and indexed types |
| Sum Types | Tagged unions without parameters |
| Product Types | Records with auto-generated projections |
| Pattern Matching | `match x in T return M with ... end` |
| Case Analysis | `case x in T return M with ... end` (no IH) |
| Equality Types | `[A]x==y` - propositional equality |
| Let Bindings | `let x: T = v in body` |
| Dot Notation | `T.field` - access projections or module members |
| Axioms | `axiom name: Type` - assume without proof |
| Examples | `example: T = value` - anonymous test values |
| Equations | `name: T, name args = value` - named equations |

## Implementation Architecture

Both Python and C implementations share identical semantics:

```
Source Code
    │
    ▼
┌─────────┐
│  Lexer   │  Tokenization
└────┬────┘
     │
     ▼
┌─────────┐
│  Parser  │  Surface AST
└────┬────┘
     │
     ▼
┌──────────────┐
│  Elaborator  │  Restricted AST → Core Terms (de Bruijn)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Type Checker │  Bidirectional type checking
└──────┬───────┘
       │
       ▼
┌─────────┐
│ Reducer  │  Beta/Iota/Delta reduction
└─────────┘
```

## Quick Start

### Python

```bash
# Type check a file
python -m Python.cli Example/Nat.pind

# Compute normal form of a global
python -m Python.cli Example/Nat.pind --nf addComm

# Launch GUI
python -m Python.gui Example/Nat.pind
```

### C

```bash
cd C
make
./test_pipeline
```

## Limitations

1. **No universe polymorphism**: Only a single `Type` universe
2. **No mutual recursion**: Each inductive type is defined independently
3. **No coinductive types**: Only finite data structures
4. **No modules/namespaces**: Dot notation is limited to product projections and eliminator names
5. **No tactic proofs**: All proofs must be explicit terms
