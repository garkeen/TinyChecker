# Type System

## Core Language

The surface syntax is elaborated into a core language using **de Bruijn indices**.

### Core Terms

```
t ::= Type              -- type universe
    | x                 -- bound variable (de Bruijn index)
    | g                 -- global reference
    | (x: t) -> t       -- dependent function type (Pi)
    | \x: t => t        -- lambda abstraction
    | t t               -- application
```

### De Bruijn Indices

Variables are represented by de Bruijn indices:
- `0` refers to the innermost binder
- `1` refers to the next binder
- And so on...

Example transformations:

```pind
# Surface syntax
\(x: A) => x

# Core (de Bruijn)
CLam "x" (CGlobal "A") (CVar 0 "x")
```

```pind
# Surface syntax
\(x: A) => \(y: B) => x

# Core (de Bruijn)
CLam "x" (CGlobal "A") (CLam "y" (CGlobal "B") (CVar 1 "x"))
```

---

## Typing Rules

### Variables

```
Γ(x) = A
──────────
Γ ⊢ x : A
```

### Type Universe

```
Γ ⊢ Type : Type
```

### Pi Type Formation

```
Γ ⊢ A : Type    Γ, x: A ⊢ B : Type
─────────────────────────────────────
Γ ⊢ (x: A) -> B : Type
```

### Lambda

```
Γ, x: A ⊢ b : B
──────────────────────────────────────
Γ ⊢ \x: A => b : (x: A) -> B
```

### Application

```
Γ ⊢ f : (x: A) -> B    Γ ⊢ a : A
─────────────────────────────────────────
Γ ⊢ f a : B[x := a]
```

---

## Reduction Rules

### Beta Reduction (Function Application)

```
(\x: A => b) a  ─β─>  b[x := a]
```

Example:

```pind
(\x: Nat => succ x) zero
  ─β─>  succ zero
```

### Iota Reduction (Eliminator Application)

When an eliminator is applied to a constructor, the corresponding branch is selected and induction hypotheses are substituted.

#### Example: Nat

```pind
inductive Nat {
  | zero: Nat
  | succ: Nat -> Nat
};
```

This introduces eliminator `Nat.rec` with type:

```
Nat.rec : (motive: Nat -> Type)
       -> (branch_zero: motive zero)
       -> (branch_succ: (k: Nat) -> motive k -> motive (succ k))
       -> (n: Nat)
       -> motive n
```

Iota reduction rules:

```
# zero case
Nat.rec motive branch_zero branch_succ zero
  ─ι─>
  branch_zero

# succ case, induction hypothesis is the recursive call
Nat.rec motive branch_zero branch_succ (succ n)
  ─ι─>
  branch_succ n (Nat.rec motive branch_zero branch_succ n)
```

#### Example: List

```pind
inductive List (A: Type): Nat -> Type {
  | nil: List A zero
  | cons: (k: Nat) -> A -> List A k -> List A (succ k)
};
```

Eliminator `List.rec`:

```
List.rec : (A: Type)
        -> (motive: (n: Nat) -> List A n -> Type)
        -> (branch_nil: motive zero (nil A))
        -> (branch_cons: (k: Nat) -> (a: A) -> (xs: List A k)
                        -> motive k xs -> motive (succ k) (cons A k a xs))
        -> (n: Nat)
        -> (xs: List A n)
        -> motive n xs
```

Iota rules:

```
List.rec A motive branch_nil branch_cons zero (nil A)
  ─ι─>
  branch_nil

List.rec A motive branch_nil branch_cons (succ k) (cons A k a xs)
  ─ι─>
  branch_cons k a xs (List.rec A motive branch_nil branch_cons k xs)
```

### Delta Reduction (Global Unfolding)

A global name is replaced by its definition:

```
# Before delta reduction
add

# After delta reduction
\(m: Nat) => \(n: Nat) => (match m in Nat return Nat -> Nat with ...)
```

---

## Definitional Equality

Two terms are **definitionally equal** if they reduce to the same normal form under the chosen strategy.

### Strategies

| Strategy | Description |
|----------|-------------|
| `greedy` | Aggressively unfold all globals, then compare structurally |
| `whnf` | Reduce both sides to WHNF, then structural compare (default) |
| `whnfv2` | Selectively expand by height, using pure beta/iota WHNF |

### WHNF (Weak Head Normal Form)

A term is in WHNF if it cannot be reduced at the head position:
- `Type`, variables, globals (without definition)
- `Pi` and `Lambda` (head binders)
- Stuck applications: `(f a1 ... an)` where `f` cannot be reduced

Examples:

```pind
# In WHNF
Type
\x: Nat => x
(Nat.rec ...) zero     # stuck: waiting for scrutinee

# NOT in WHNF
(\x: Nat => x) zero    # can beta-reduce
add zero n             # can delta-reduce add
```

### NF (Normal Form)

A term is in NF when all subterms are in NF:

```pind
# In NF
Type
\x: Nat => succ zero
succ (succ zero)

# NOT in NF
\x: Nat => (\y: Nat => y) x    # inner beta-redex
```

---

## Inductive Types

### Formation

An inductive type declaration introduces:
1. A **type constructor** with parameters and indices
2. **Data constructors** for each variant
3. An **eliminator** (recursor or case analyzer)

### Parameters vs Indices

```pind
inductive List (A: Type): Nat -> Type {
#               ^^^^^^^^   ^^^^^^^^^^
#               parameter  index
  | nil: List A zero
  | cons: (k: Nat) -> A -> List A k -> List A (succ k)
};
```

- **Parameters** (`A`): Same across all constructors, appear uniformly
- **Indices** (`Nat`): Can vary between constructors, determine the type family

### Constructor Rules

1. **Positive occurrence**: The type being defined must appear only in positive positions
2. **Uniform parameters**: Parameters must be passed unchanged to recursive occurrences
3. **Correct indices**: Each constructor must return the type with valid indices

### Eliminator Structure

For an inductive type `T` with parameters `P` and indices `I`:

```
T.rec : P_1 -> ... -> P_n           -- parameters
     -> motive                       -- return type motive
     -> branch_1 -> ... -> branch_k  -- one branch per constructor
     -> I_1 -> ... -> I_m           -- indices
     -> scrutinee                    -- value to eliminate
     -> result                       -- motive applied to indices and scrutinee
```

Where `motive` has type:

```
motive : I_1 -> ... -> I_m -> T P_1 ... P_n I_1 ... I_m -> Type
```

### Induction Hypotheses

For recursive fields, the eliminator automatically generates induction hypotheses:

```pind
match n as q in Nat return Nat with
| zero => ...              # no IH for non-recursive constructor
| succ k ih => ...         # ih : motive(k), the result of recursive call
end
```

The IH has the type specified by the motive applied to the recursive field's arguments.

For higher-order recursive fields:

```pind
| node k childAt [ihAt] =>
    # ihAt : (i: Fin k) -> motive (childAt i)
    ...
end
```

---

## Sum Types

Non-parameterized tagged unions. Case analysis does **not** provide induction hypotheses.

```pind
sum Bool {
  | true: Bool
  | false: Bool
};

# Bool.elim : motive -> branch_true -> branch_false -> Bool -> result
```

Usage:

```pind
case b in Bool return Bool with
| true => false
| false => true
end
```

---

## Product Types

Record types with a single constructor and automatic projections.

```pind
product Pair {
  fst: Nat,
  snd: Nat
};
```

Generates:
- `Pair.mk : Nat -> Nat -> Pair` (constructor)
- `Pair.elim : motive -> branch -> Pair -> result` (eliminator)
- `Pair.fst : Pair -> Nat` (projection)
- `Pair.snd : Pair -> Nat` (projection)

### Projection Implementation

Projections are implemented via case analysis:

```
Pair.fst = \p: Pair => Pair.elim (\_: Pair => Nat) (\fst: Nat => fst) p
```

---

## Equality Types

### Formation

```pind
inductive Eq (A: Type): A -> A -> Type {
  | refl: (x: A) -> Eq A x x
};
```

### Syntax Sugar

```pind
[A]x==y    # desugars to Eq A x y
```

### Introduction

```pind
refl Nat zero : [Nat]zero==zero
```

### Elimination

```pind
# Transport: if x = y, then P x implies P y
eqTransport : (A: Type) -> (x: A) -> (y: A) -> [A]x==y
           -> (P: A -> Type) -> P x -> P y
```

### Congruence

```pind
# If x = y, then f x = f y
eqCong : (A: Type) -> (x: A) -> (y: A) -> [A]x==y
      -> (B: Type) -> (f: A -> B) -> [B](f x)==(f y)
```

---

## Universe Hierarchy

Pind has a single universe `Type` (aliased as `Prop`). There is no universe polymorphism.

```pind
# This is valid:
Type : Type

# But be careful of logical consistency!
# The axiom of Type: Type can lead to Girard's paradox.
```

---

## Reduction Examples

### Example 1: Addition

```pind
fun add (m: Nat) (n: Nat): Nat {
  (match m in Nat
   return Nat -> Nat with
   | zero => \(n: Nat) => n
   | succ k [rec] => \(n: Nat) => succ (rec n)
   end)
  n
};

# add zero n reduces as:
#   (Nat.rec ... zero) n
#   ─ι─> (\(n: Nat) => n) n
#   ─β─> n

# add (succ k) n reduces as:
#   (Nat.rec ... (succ k)) n
#   ─ι─> (\(k: Nat) (rec: Nat -> Nat) => \(n: Nat) => succ (rec n)) k (Nat.rec ... k) n
#   ─β─> (\(rec: Nat -> Nat) => \(n: Nat) => succ (rec n)) (Nat.rec ... k) n
#   ─β─> (\(n: Nat) => succ ((Nat.rec ... k) n)) n
#   ─β─> succ ((Nat.rec ... k) n)
```

### Example 2: List Length

```pind
fun ListLength (A: Type) (n: Nat) (L: List A n): Nat {
  match L in List A n bind T k
  return Nat with
  | nil => zero
  | cons k a xs [ih] => succ ih
  end
};

# ListLength A zero (nil A) reduces as:
#   List.rec ... zero (nil A)
#   ─ι─> zero

# ListLength A (succ k) (cons A k a xs) reduces as:
#   List.rec ... (succ k) (cons A k a xs)
#   ─ι─> succ (List.rec ... k xs)
```

---

## Strict Positivity

The type being defined must only appear in **positive positions** within constructor argument types.

### Positive Position

```pind
# OK: Nat appears positively
inductive Tree {
  | leaf: Nat -> Tree
  | node: Tree -> Tree -> Tree
};

# OK: Nested positive occurrence
inductive Acc (R: A -> A -> Type) (x: A) {
  | acc: ((y: A) -> R y x -> Acc R y) -> Acc R x
};
```

### Negative Position (Forbidden)

```pind
# NOT OK: A appears negatively in (A -> ...)
inductive Bad {
  | bad: (Bad -> Nat) -> Bad
};
```

This restriction ensures logical consistency (prevents paradoxes like Russell's paradox).
