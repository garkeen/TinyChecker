# Type System

## Core Language

The surface syntax is elaborated into a core language using de Bruijn indices.

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

Variables are represented by de Bruijn indices: `0` refers to the innermost binder, `1` to the next, etc.

Example: `\(x: A) => x` becomes `CLam "x" (CGlobal "A") (CVar 0 "x")`

## Typing Rules

### Variables

```
Γ(x) = A
──────────
Γ ⊢ x : A
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
──────────────────────────────
Γ ⊢ \x: A => b : (x: A) -> B
```

### Application

```
Γ ⊢ f : (x: A) -> B    Γ ⊢ a : A
─────────────────────────────────────
Γ ⊢ f a : B[x := a]
```

### Type

```
Γ ⊢ Type : Type
```

## Reduction Rules

### Beta Reduction (Function Application)

```
(\x: A => b) a  ─β─>  b[x := a]
```

### Iota Reduction (Eliminator Application)

When an eliminator is applied to a constructor, the corresponding branch is selected and induction hypotheses are substituted.

Consider the inductive type:

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

Example reduction:

```pind
# Define add using eliminator
fun add (m: Nat) (n: Nat): Nat {
  (Nat.rec (\(_: Nat) => Nat -> Nat)
           (\(n: Nat) => n)
           (\(k: Nat) (rec: Nat -> Nat) => \(n: Nat) => succ (rec n))
           m)
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

### Delta Reduction (Global Unfolding)

A global name is replaced by its definition:

```
# Before
add

# After delta reduction
\(m:Nat) => \(n:Nat) => (match m in Nat return Nat -> Nat with ...)
```

## Definitional Equality

Two terms are definitionally equal if they reduce to the same normal form.

### Strategies

| Strategy | Description |
|----------|-------------|
| `greedy` | Aggressively unfold all globals, then compare |
| `whnf` | Reduce both sides to WHNF, then structural compare (default) |
| `whnfv2` | Selectively expand by height, using pure beta/iota WHNF |

### WHNF (Weak Head Normal Form)

A term is in WHNF if it cannot be reduced at the head position:
- `Type`, variables, globals (without definition)
- `Pi` and `Lambda` (head binders)
- Stuck applications: `(f a1 ... an)` where `f` cannot be reduced

### NF (Normal Form)

A term is in NF when all subterms are in NF.

## Inductive Types

### Formation

An inductive type declaration introduces:
- A **type constructor** with parameters and indices
- **Data constructors** for each variant
- An **eliminator** (recursor or case analyzer)

### Example

```pind
inductive Vec (A: Type) : Nat -> Type {
| nil: Vec A zero
| cons: (n: Nat) -> A -> Vec A n -> Vec A (succ n)
};
```

Introduces:
- `Vec : Type -> Nat -> Type` (type constructor)
- `nil : Vec A zero` (data constructor)
- `cons : (A: Type) -> (n: Nat) -> A -> Vec A n -> Vec A (succ n)` (data constructor)
- `Vec.rec : (A: Type) -> (motive: (n: Nat) -> Vec A n -> Type) -> ...` (eliminator)

### Eliminator Structure

For an inductive type `T` with parameters `P` and indices `I`:

```
T.rec : P -> motive -> branch_1 -> ... -> branch_n -> I -> T P I -> result
```

Where:
- `motive` specifies the return type for each index and value
- Each `branch_i` handles one constructor, with induction hypotheses for recursive fields

### Induction Hypotheses

For recursive fields, the eliminator automatically generates induction hypotheses. The `[ih_name]` syntax binds the induction hypothesis in each branch:

```pind
match n as q in Nat return Nat with
| zero => ...              # no IH for non-recursive constructor
| succ k ih => ...         # ih : motive(k), the result of recursive call
end
```

The induction hypothesis has the type specified by the motive applied to the recursive field's arguments.

## Sum Types

Non-parameterized tagged unions. Case analysis does not provide induction hypotheses.

```pind
sum Bool {
| true: Bool
| false: Bool
};

# Bool.elim : motive -> branch_true -> branch_false -> Bool -> result
```

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