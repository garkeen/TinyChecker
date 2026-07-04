# TinyChecker Manual

**A dependently-typed language type checker and proof assistant for the Pind language.**

## Table of Contents

| Document | Description |
|----------|-------------|
| [overview.md](overview.md) | System overview, core concepts, quick start |
| [syntax.md](syntax.md) | Complete syntax specification (EBNF) |
| [types.md](types.md) | Type system, reduction rules, definitional equality |
| [modules.md](modules.md) | Implementation architecture, module responsibilities |
| [api.md](api.md) | CLI and Python/C API reference |
| [errors.md](errors.md) | Error types and debugging guide |

## Quick Links

### Language Features
- [Pi Types](syntax.md#pi-type-dependent-function-type) — `(x: A) -> B`
- [Lambda](syntax.md#lambda) — `\(x: A) => body`
- [Inductive Types](syntax.md#3-inductive-type) — `inductive T { ... }`
- [Sum Types](syntax.md#4-sum-type) — `sum T { ... }`
- [Product Types](syntax.md#5-product-type) — `product T { ... }`
- [Pattern Matching](syntax.md#match-expression) — `match x in T return M with ... end`
- [Equality](syntax.md#equality-type) — `[A]x==y`

### Implementation
- [Core Terms](modules.md#1-core-terms-corepy--termhc) — de Bruijn representation
- [Elaboration](modules.md#6-elaborator-elaboratorpy--elaboratorhc) — surface → core translation
- [Type Checking](modules.md#9-type-checker-typecheckerpy--typecheckerhc) — bidirectional checking
- [Reduction](modules.md#8-reducer-reducerpy--reducerhc) — beta/iota/delta reduction

### Examples
- [Nat.pind](../Example/Nat.pind) — Natural numbers, arithmetic proofs
- [List.pind](../Example/List.pind) — Indexed lists, length proofs
- [Tree.pind](../Example/Tree.pind) — Binary trees, Fin type
- [Sort.pind](../Example/Sort.pind) — Insertion sort, sorted proofs
- [PropositionLogic.pind](../Example/PropositionLogic.pind) — Propositional logic
- [FirstOrderlogic.pind](../Example/FirstOrderlogic.pind) — First-order logic

## Getting Started

### 1. Type Check a File

```bash
python -m Python.cli Example/Nat.pind
```

### 2. Compute Normal Form

```bash
python -m Python.cli Example/Nat.pind --nf addComm
```

### 3. Launch GUI

```bash
python -m Python.gui Example/Nat.pind
```

## Language Summary

```pind
# Define a type
inductive Nat {
  | zero: Nat
  | succ: Nat -> Nat
};

# Define a function using pattern matching
fun add (m: Nat) (n: Nat): Nat {
  (match m in Nat
   return Nat -> Nat with
   | zero => \(n: Nat) => n
   | succ k [rec] => \(n: Nat) => succ (rec n)
   end)
  n
};

# Prove a theorem by induction
theorem addZeroRight (n: Nat): [Nat](add n zero)==n {
  match n as k in Nat
  return [Nat](add k zero)==k with
  | zero => refl Nat zero
  | succ k [ih] => eqCongSucc (add k zero) k ih
  end
};
```

## Key Concepts

1. **Dependent Types**: Types can depend on values
2. **Propositions as Types**: Proofs are terms of their type
3. **Induction via Eliminators**: All recursion through `match`
4. **Induction Hypotheses**: `[ih]` in recursive branches
5. **Definitional Equality**: Computation during type checking
