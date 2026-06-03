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

`Prop` is treated as `Type` during elaboration.

### Identifiers

- Regular: `[a-zA-Z_][a-zA-Z0-9_]*`
- Dot notation: `A.b` (exactly one dot, second part must start with letter or underscore)

### Operators

| Token | Symbol |
|-------|--------|
| ARROW | `->` |
| DARROW | `=>` |
| EQEQ | `==` |

### Delimiters

```
{ } ( ) [ ] < > : = \ ; , |
```

## Grammar (EBNF)

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

var_decl       = ("var" | "claim") IDENT ":" expr "=" expr

fun_decl       = ("fun" | "theorem") IDENT param* ":" expr "{" expr "}"

inductive_decl = "inductive" IDENT param* (":" expr)? ctor_block

sum_decl       = "sum" IDENT ctor_block

product_decl   = "product" IDENT "{" field_decl ("," field_decl)* "}"

axiom_decl     = "axiom" IDENT ":" expr

example_decl   = "example" ":" expr "=" expr

equation_decl  = IDENT ":" expr "," IDENT bind_name* "=" expr

param          = "(" IDENT ":" expr ")"

field_decl     = IDENT ":" expr

ctor_block     = "{" ("|" IDENT ":" expr)+ "}"

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

## Operator Precedence

From lowest to highest:

1. `->` (arrow)
2. Application (juxtaposition)
3. Atoms

## Examples

### Inductive Type

```pind
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};
```

### Function via Eliminator

Functions are defined using eliminators, not direct recursion. `[rec]` is the induction hypothesis provided by the eliminator.

```pind
fun add (m: Nat) (n: Nat): Nat {
  (match m in Nat
   return Nat -> Nat with
   | zero => \(n:Nat) => n
   | succ k [rec] => \(n:Nat) => succ (rec n)
   end)
  n
};
```

### Sum Type with Case Analysis

```pind
sum Bool {
| true: Bool
| false: Bool
};

fun not (b: Bool): Bool {
  case b in Bool of
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
```

Access projections as `Pair.fst` and `Pair.snd`.

### Pi Type and Lambda

```pind
var id: (A: Type) -> A -> A = \(A: Type) => \(x: A) => x;
```