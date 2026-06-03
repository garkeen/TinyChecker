# System Overview

## What is Pind?

Pind is a dependently-typed language type checker. It verifies that programs are well-typed according to the rules of dependent type theory with inductive types.

## Input / Output

- **Input**: `.pind` source files containing declarations and expressions
- **Output**: Type-checking result (OK + global context, or Error + diagnostic)

## Supported Features

### Type Constructors

| Feature | Syntax | Description |
|---------|--------|-------------|
| Pi type | `(x: A) -> B` | Dependent function type |
| Inductive type | `inductive` | Parameterized, indexed families |
| Sum type | `sum` | Non-parameterized tagged unions |
| Product type | `product` | Record types with auto-generated projections |

### Expressions

| Feature | Syntax | Description |
|---------|--------|-------------|
| Lambda | `\(x: A) => e` | Function abstraction |
| Application | `f x` | Function application |
| Let binding | `let x: A = e1 in e2` | Local definition |
| Match | `match e as q in T args bind ... return motive with branches end` | Pattern matching on inductive types |
| Case | `case e as q in T args bind ... return motive of branches end` | Case analysis on sum/product types |
| Equality | `[A] a == b` | Equality type |

### Declarations

| Keyword | Description |
|---------|-------------|
| `var` / `claim` | Variable declaration |
| `fun` / `theorem` | Function / theorem definition |
| `axiom` | Assumed constant (no body) |
| `example` | Anonymous test expression |
| `equation` | Named equation assertion |

### Inductive Features

- Constructors with uniform parameters
- Recursive fields (direct and higher-order)
- Eliminators with induction hypotheses
- Automatic projection generation for product types

## Limitations

| Not Supported | Description |
|---------------|-------------|
| Universe polymorphism | Single `Type` universe only |
| Recursive definitions | No `fix`; recursion only via eliminators |
| Mutual inductive types | Not supported |
| Coinductive types | Not supported |
| Implicit arguments | All arguments must be explicit |
| Type inference | Type annotations required |
| Strict positivity check | Not enforced |
| Pattern match completeness | Not checked |
| Termination check | Relies on eliminators |
| Module system | No namespaces or imports |
| Higher-order inductive types | Not supported |