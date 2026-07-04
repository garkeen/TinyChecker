# Syntax Specification

## Lexical Structure

### Comments

```
# This is a comment, from # to end of line
```

### Keywords

```
var  fun  claim  theorem  inductive  sum  product  axiom  example
let  in  match  case  as  bind  return  with  of  end
```

### Type Keywords

```
Type  Prop
```

`Prop` is treated as `Type` during elaboration. They are semantically identical.

### Identifiers

- **Regular**: `[a-zA-Z_][a-zA-Z0-9_]*`
  - Examples: `Nat`, `add_comm`, `_x`, `x1`
- **Dot notation**: `A.b` (exactly one dot, second part must start with letter or underscore)
  - Examples: `Pair.fst`, `Nat.rec`, `List.cons`

### Operators

| Token | Symbol | Description |
|-------|--------|-------------|
| ARROW | `->` | Function type (right-associative) |
| DARROW | `=>` | Lambda body separator |
| EQEQ | `==` | Equality in `[T]x==y` |

### Delimiters

```
{ } ( ) [ ] < > : = \ ; , |
```

---

## Grammar (EBNF)

### Program Structure

```ebnf
program        = decl (";" decl)* ";"?

decl           = var_decl
               | fun_decl
               | inductive_decl
               | sum_decl
               | product_decl
               | axiom_decl
               | example_decl
               | equation_decl
```

### Declarations

```ebnf
var_decl       = ("var" | "claim") IDENT ":" expr "=" expr

fun_decl       = ("fun" | "theorem") IDENT param* ":" expr "{" expr "}"

inductive_decl = "inductive" IDENT param* (":" expr)? ctor_block

sum_decl       = "sum" IDENT ctor_block

product_decl   = "product" IDENT "{" field_decl ("," field_decl)* "}"

axiom_decl     = "axiom" IDENT ":" expr

example_decl   = "example" ":" expr "=" expr

equation_decl  = IDENT ":" expr "," IDENT bind_name* "=" expr
```

### Parameters and Fields

```ebnf
param          = "(" IDENT ":" expr ")"

field_decl     = IDENT ":" expr

ctor_block     = "{" ("|" IDENT ":" expr)+ "}"
               | "{" "}"                             -- empty block
```

### Expressions

```ebnf
expr           = expr "->" expr                   -- arrow (right-assoc)
               | app_expr                          -- application

app_expr       = atom+                            -- left-assoc application

atom           = IDENT                            -- variable or global reference
               | DOT                              -- dot-notation identifier
               | "Type"                           -- type universe
               | "Prop"                           -- proposition (alias for Type)
               | "(" IDENT ":" expr ")" "->" expr -- pi type
               | "(" expr ")"                     -- parenthesized expression
               | "\" param "=>" expr              -- lambda abstraction
               | "let" IDENT ":" expr "=" expr "in" expr
               | match_expr
               | case_expr
               | "[" expr "]" expr "==" expr      -- equality type
               | IDENT "<" expr ("," expr)* ">"   -- product expression
```

### Pattern Matching

```ebnf
match_expr     = "match" expr ("as" IDENT)? "in" IDENT expr*
                 ("bind" IDENT*)? "return" expr "with"
                 match_branch+ "end"

case_expr      = "case" expr ("as" IDENT)? "in" IDENT expr*
                 ("bind" IDENT*)? "return" expr "of"
                 case_branch+ "end"

match_branch   = "|" IDENT bind_name* ("[" bind_name* "]")? "=>" expr

case_branch    = "|" IDENT bind_name* "=>" expr

bind_name      = IDENT | "_"
```

---

## Operator Precedence

From lowest to highest:

| Precedence | Operator | Associativity |
|------------|----------|---------------|
| 1 (lowest) | `->` | Right |
| 2 | Application (juxtaposition) | Left |
| 3 (highest) | Atoms | - |

---

## Syntax Details

### 1. Variable Declaration

```pind
var x: Nat = succ zero;
claim y: Nat = succ zero;    # claim is an alias for var
```

Both `var` and `claim` introduce a global definition with a name, type, and value.

### 2. Function Declaration

```pind
fun add (m: Nat) (n: Nat): Nat {
  body_expression
};

theorem addComm (m: Nat) (n: Nat): [Nat](add m n)==(add n m) {
  proof_term
};
```

- `fun` and `theorem` are semantically identical
- Parameters are in parentheses with explicit types
- Return type comes after `:`
- Body is enclosed in `{ }`
- Optional trailing `;`

### 3. Inductive Type

```pind
# Simple type
inductive Nat {
  | zero: Nat
  | succ: Nat -> Nat
};

# Type with parameters
inductive Eq (A: Type): A -> A -> Type {
  | refl: (x: A) -> Eq A x x
};

# Type with indices
inductive List (A: Type): Nat -> Type {
  | nil: List A zero
  | cons: (k: Nat) -> A -> List A k -> List A (succ k)
};

# Empty type
inductive Empty {};
```

- Parameters are in `(name: Type)` before `:`
- Indices are in the return type after `:`
- Constructors are separated by `|`
- Empty constructors block `{}` is valid

### 4. Sum Type

```pind
sum Bool {
  | true: Bool
  | false: Bool
};
```

- No parameters allowed
- Used with `case` expression (no induction hypotheses)

### 5. Product Type

```pind
product Pair {
  fst: Nat,
  snd: Nat
};
```

Generates:
- `Pair.mk : Nat -> Nat -> Pair` (constructor)
- `Pair.fst : Pair -> Nat` (projection)
- `Pair.snd : Pair -> Nat` (projection)
- `Pair.elim` (eliminator)

### 6. Axiom

```pind
axiom classic: (P: Type) -> Or P (P -> Empty);
```

Assumes a term exists without providing a proof. Use with caution.

### 7. Example

```pind
example: Nat = succ zero;
```

Anonymous test value. Gets a generated name like `example_1`.

### 8. Equation

```pind
addComm: (m: Nat) -> (n: Nat) -> [Nat](add m n)==(add n m),
addComm m n = proof_term;
```

Format: `name: type, name params = value`

---

## Expression Syntax

### Pi Type (Dependent Function Type)

```pind
(A: Type) -> A -> A                    # dependent
(x: Nat) -> [Nat]x==x                  # dependent on value
Nat -> Nat -> Nat                      # non-dependent (shorthand)
```

### Lambda

```pind
\(A: Type) => \(x: A) => x
\(n: Nat) => succ n
\(x: Nat) => \(y: Nat) => add x y
```

### Application

```pind
f x y                  # left-associative
add m n
eqCong Nat x y p Nat f
```

### Let Expression

```pind
let x: Nat = zero in succ x
let f: Nat -> Nat = \(n: Nat) => succ n in f zero
```

### Equality Type

```pind
[Nat]x==y              # Eq Nat x y
[A](f x)==(f y)        # Eq A (f x) (f y)
```

### Product Expression

```pind
Pair<zero, zero>       # Construct a Pair
```

### Match Expression

```pind
match scrutinee [as alias] in TypeFamily args...
[bind index_names...]
return motive
with
| Ctor1 fields... [ih...] => body1
| Ctor2 fields... [ih...] => body2
end
```

Example:

```pind
match n as k in Nat
return [Nat](add k zero)==k with
| zero => refl Nat zero
| succ k [ih] => eqCongSucc (add k zero) k ih
end
```

### Case Expression

```pind
case scrutinee [as alias] in SumType args...
[bind index_names...]
return motive
with
| Ctor1 fields... => body1
| Ctor2 fields... => body2
end
```

Example:

```pind
case b in Bool
return Bool with
| true => false
| false => true
end
```

---

## Bind Syntax

The `bind` clause explicitly names variables in the type family's indices:

```pind
match p as e in Eq A x y bind T lhs rhs
return [T]lhs==rhs with
| refl z => refl T z
end
```

Here:
- `T` binds to `A` (the type parameter)
- `lhs` binds to `x`
- `rhs` binds to `y`

These names can then be used in the `return` motive and branches.

---

## Induction Hypothesis Syntax

In `match` branches, `[names...]` binds induction hypotheses for recursive fields:

```pind
match n as k in Nat
return Nat -> Nat with
| zero => \(n: Nat) => n
| succ k [rec] => \(n: Nat) => succ (rec n)
end
```

- `[rec]` corresponds to the recursive argument `k`
- `rec` has type `Nat -> Nat` (the motive applied to `k`)
- Multiple recursive fields produce multiple IH names

For higher-order recursive fields:

```pind
| node k childAt [ihAt] =>
    succ (sumFin k (\(i: Fin k) => ihAt i))
```

- `ihAt` is a function from `Fin k` to the motive result

---

## Examples

### Inductive Type with Eliminator

```pind
inductive Nat {
  | zero: Nat
  | succ: Nat -> Nat
};

fun add (m: Nat) (n: Nat): Nat {
  (match m in Nat
   return Nat -> Nat with
   | zero => \(n: Nat) => n
   | succ k [rec] => \(n: Nat) => succ (rec n)
   end)
  n
};
```

### Indexed Type

```pind
inductive List (A: Type): Nat -> Type {
  | nil: List A zero
  | cons: (k: Nat) -> A -> List A k -> List A (succ k)
};

theorem ListLengthSound (A: Type) (n: Nat) (L: List A n):
  [Nat]n==(ListLength A n L) {
  match L as xs0 in List A n bind T k
  return [Nat]k==(ListLength T k xs0) with
  | nil => refl Nat zero
  | cons k a xs [ih] => eqCongSucc k (ListLength T k xs) ih
  end
};
```

### Sum Type with Case

```pind
sum Bool {
  | true: Bool
  | false: Bool
};

fun not (b: Bool): Bool {
  case b in Bool
  return Bool with
  | true => false
  | false => true
  end
};
```

### Product Type

```pind
product Pair {
  fst: Nat,
  snd: Nat
};

var p: Pair = Pair<zero, zero>;
var first: Nat = Pair.fst p;
```

### Propositional Logic

```pind
inductive Or (P: Type) (Q: Type) {
  | inl: P -> Or P Q
  | inr: Q -> Or P Q
};

theorem disjSyll (P: Type) (Q: Type) (pq: Or P Q) (notP: P -> Empty): Q {
  (match pq
   in Or P Q bind X Y
   return (X -> Empty) -> Y with
   | inl p => \(np: X -> Empty) => exfalso Y (np p)
   | inr q => \(np: X -> Empty) => q
   end)
  notP
};
```
