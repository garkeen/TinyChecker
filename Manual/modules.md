# Module Structure

## Python Implementation

```
Python/
├── __init__.py
├── core.py           # Core term representation (de Bruijn)
├── core_ops.py       # De Bruijn operations (shift, subst, instantiate)
├── surface.py        # Surface syntax AST
├── lexer.py          # Tokenizer
├── parser.py         # Parser
├── elaborator.py     # Elaboration (surface → restricted)
├── restricted.py     # Restricted expressions → core terms
├── reducer.py        # Reduction engine (beta, iota, delta)
├── typechecker.py    # Bidirectional type checker
├── pipeline.py       # End-to-end pipeline
├── pretty.py         # Pretty printer
├── runtime.py        # Runtime global context
├── errors.py         # Error types
├── cli.py            # Command-line interface
├── gui.py            # GUI (PySide6)
└── Test/             # Python tests
```

## C Implementation

```
C/
├── term.h/c          # Core term (de Bruijn)
├── core_ops.h/c      # shift, subst, instantiate
├── surface.h/c       # Surface AST
├── lexer.h/c         # Tokenizer
├── parser.h/c        # Parser
├── elaborator.h/c    # Elaboration
├── restricted.h/c    # Restricted → core conversion
├── reducer.h/c       # Reduction engine
├── typechecker.h/c   # Type checker
├── pipeline.h/c      # End-to-end pipeline
├── runtime.h/c       # Global context
├── context.h/c       # Typing context
├── pretty.h/c        # Pretty printer
├── cdecl.h/c         # Core declaration types
├── Makefile          # Build system
└── Test/             # C tests
```

---

## Data Flow

```
Source Code (string)
       │
       ▼
┌─────────────┐
│    Lexer     │  lexer.py / lexer.h/c
└──────┬──────┘
       │ tokens: list[Token]
       ▼
┌─────────────┐
│    Parser    │  parser.py / parser.h/c
└──────┬──────┘
       │ decls: list[Decl]  (Surface AST)
       ▼
┌──────────────┐
│  Elaborator  │  elaborator.py / elaborator.h/c
└──────┬───────┘
       │ rdecls: list[RDecl]  (Restricted AST)
       │ core_decls: list[CDecl]  (Core declarations)
       ▼
┌──────────────┐
│ Type Checker │  typechecker.py / typechecker.h/c
└──────┬───────┘
       │ global_ctx: TcGlobalContext / GlobalContext
       ▼
┌─────────────┐
│   Reducer    │  reducer.py / reducer.h/c
└─────────────┘
       │ Used for definitional equality checking
```

---

## Module Responsibilities

### 1. Core Terms (`core.py` / `term.h/c`)

Immutable representation of terms using de Bruijn indices.

**Types:**
- `CVar(index, name)` — bound variable
- `CGlobal(name)` — global reference
- `CType()` — type universe
- `CPi(name, domain, codomain)` — dependent function type
- `CLam(name, param_type, body)` — lambda abstraction
- `CApp(func, arg)` — application

**Declarations:**
- `CDefinition(name, typ, value, kind)` — global definition
- `CTypeCtorDecl(...)` — type constructor
- `CDataCtorDecl(...)` — data constructor
- `CEliminatorDecl(...)` — eliminator

**Helpers:**
- `unfold_app(term)` — decompose `App(App(f, a1), a2)` into `(f, [a1, a2])`
- `mk_apps(head, args)` — build application chain

### 2. Core Operations (`core_ops.py` / `core_ops.h/c`)

De Bruijn index manipulation.

- `shift(term, amount, cutoff)` — shift free variables
- `subst(term, index, replacement)` — substitute variable
- `instantiate(body, arg)` — beta reduction step
- `instantiate_many(body, args)` — multiple instantiations
- `instantiate_env(term, env, depth)` — environment-based instantiation

### 3. Surface AST (`surface.py` / `surface.h/c`)

Concrete syntax tree before elaboration.

**Expressions:**
- `AtomExpr(text)` — identifier or keyword
- `ArrowExpr(domain, codomain)` — non-dependent function type
- `PiExpr(name, domain, codomain)` — dependent function type
- `LambdaExpr(param, body)` — lambda abstraction
- `AppExpr(func, arg)` — application
- `LetExpr(name, typ, value, body)` — let binding
- `EqExpr(typ, lhs, rhs)` — equality type `[T]a==b`
- `MatchExpr(...)` — pattern matching on inductive types
- `CaseExpr(...)` — case analysis on sum types
- `ProductExpr(type_name, args)` — product value construction

**Declarations:**
- `VarDecl(kind, name, typ, value)` — variable/claim declaration
- `FunDecl(kind, name, params, ret_type, body)` — function/theorem
- `InductiveDecl(kind, name, params, arity, ctors)` — inductive type
- `ProductDecl(name, fields)` — product type
- `AxiomDecl(name, typ)` — axiom
- `ExampleDecl(typ, value)` — example
- `EquationDecl(name, typ, params, value)` — equation

### 4. Lexer (`lexer.py` / `lexer.h/c`)

Tokenizes source code into a flat list of tokens.

**Token types:**
- Keywords: `VAR`, `FUN`, `THEOREM`, `INDUCTIVE`, `SUM`, `PRODUCT`, etc.
- Type keywords: `TYPE`, `PROP`
- Operators: `ARROW` (`->`), `DARROW` (`=>`), `EQEQ` (`==`)
- Delimiters: `LBRACE`, `RBRACE`, `LPAREN`, `RPAREN`, etc.
- Identifiers: `IDENT`, `DOT` (`Module.name`)
- Special: `EOF`

**Features:**
- Comment handling (`#` to end of line)
- Bracket matching validation
- Tab width handling (2 spaces)

### 5. Parser (`parser.py` / `parser.h/c`)

Transforms token stream into Surface AST.

**Key functions:**
- `parse_decl()` — dispatch to specific declaration parsers
- `parse_expr(stop)` — parse expression with stop set
- `parse_app(stop)` — parse application chain
- `parse_match_expr()` — parse match with branches
- `parse_case_expr()` — parse case with branches

**Metadata tracking (Python only):**
- `node_spans` — source positions for each AST node
- `name_sites` — locations of name bindings (for GUI)

### 6. Elaborator (`elaborator.py` / `elaborator.h/c`)

Transforms Surface AST into Restricted AST and Core declarations.

**Key responsibilities:**
- Name resolution (local vs global)
- Dot notation resolution (`Nat.rec`, `Pair.fst`)
- Telescope construction (parameter lists)
- Inductive type family compilation
- Match/case compilation to eliminator applications
- Induction hypothesis generation
- Product projection generation

**State:**
- `global_tags` — registered global names
- `inductives` — inductive type specifications
- `example_counter` — unique name generation

### 7. Restricted Expressions (`restricted.py` / `restricted.h/c`)

Intermediate representation between Surface and Core.

**Types:**
- `RDefinition(name, typ, value, kind)`
- `RTypeCtorDecl(name, param_telescope, index_telescope, ...)`
- `RDataCtorDecl(name, owner, typ, ...)`
- `REliminatorDecl(name, owner, motive_type, typ, ...)`

**Operations:**
- `fold_pi_expr(telescope, body)` — build Pi chain from telescope
- `fold_lam_expr(telescope, body)` — build Lambda chain
- `split_pi_expr(expr)` — decompose Pi chain into telescope
- `substitute_expr(expr, mapping)` — capture-avoiding substitution
- `restricted_expr_to_core(expr, local)` — convert to de Bruijn
- `restricted_decl_to_core(rdecl)` — convert declaration

### 8. Reducer (`reducer.py` / `reducer.h/c`)

Reduction engine for definitional equality.

**Strategies:**
- `greedy` — unfold everything, compare
- `whnf` — WHNF then structural compare (default)
- `whnfv2` — selective unfolding by dependency height

**Operations:**
- `whnf(term)` — weak head normal form
- `nf(term)` — full normal form
- `is_def_eq(lhs, rhs)` — definitional equality check
- `step_head(term)` — single head reduction step

**Reduction rules:**
- Beta: `(\x => b) a → b[x := a]`
- Iota: eliminator applied to constructor
- Delta: unfold global definition

### 9. Type Checker (`typechecker.py` / `typechecker.h/c`)

Bidirectional type checking.

**Modes:**
- `infer(term, ctx)` — synthesize type
- `check(term, expected, ctx)` — check against expected type

**Declaration checking:**
- `check_definition(decl)` — check var/claim
- `check_type_ctor(decl)` — register type constructor
- `check_data_ctor(decl)` — register data constructor
- `check_eliminator(decl)` — register eliminator

**Context:**
- Linked list of `(name, type)` pairs
- O(1) extend, O(n) lookup by de Bruijn index

### 10. Runtime (`runtime.py` / `runtime.h/c`)

Global context for type checking and reduction.

**Entries:**
- `GlobalEntry(name, typ, value, kind)` — any global definition
- `InductiveInfo(name, kind, param_count, index_count, ...)` — type info
- `ConstructorInfo(name, owner, typ, param_count, fields, ...)` — ctor info
- `EliminatorInfo(name, owner, typ, branch_order, ...)` — eliminator info

### 11. Pretty Printer (`pretty.py` / `pretty.h/c`)

Converts Core terms back to readable strings.

- `show_term(term)` — term to string

### 12. Pipeline (`pipeline.py` / `pipeline.h/c`)

End-to-end processing.

```python
def run_pipeline(source, conv_strategy="whnf"):
    tokens = tokenize(source)
    decls = parse(tokens)
    elab = elaborate_result(decls)
    global_ctx = check_program(elab.core_decls, conv_strategy=conv_strategy)
    return tokens, decls, elab.core_decls, global_ctx
```

### 13. Errors (`errors.py`)

Exception hierarchy:

```
TinyCheckerError
├── LexError
├── ParseError
├── ElabError
├── TypeCheckError
└── ReducerError
```

### 14. Context (`context.h/c`)

Typing context as a linked list.

```c
typedef struct CtxNode {
    const Term* ty;
    const char* name;
    struct CtxNode* tail;
} CtxNode;

typedef struct {
    CtxNode* head;
    uint32_t depth;
} Context;
```

### 15. C Declarations (`cdecl.h/c`)

Core declaration types for C implementation.

- `CDeclKind` — enum of declaration kinds
- `CRecursiveFieldDecl` — recursive field info
- `CDefinition`, `CTypeCtorDecl`, `CDataCtorDecl`, `CEliminatorDecl`

---

## Python vs C Comparison

Both implementations have **identical semantics**. Key differences:

| Aspect | Python | C |
|--------|--------|---|
| Memory management | GC (frozen dataclasses) | Manual malloc/free |
| Data structures | Dicts, lists | Fixed-size arrays with counts |
| Error handling | Exceptions | Return codes + error messages |
| String handling | Python str | `const char*` with `strdup` |
| Pattern matching | `isinstance` | `switch` on kind enum |

### Functional Equivalence

All core functions are semantically identical:

| Python | C | Status |
|--------|---|--------|
| `shift(term, amount, cutoff)` | `term_shift(term, amount, cutoff)` | ✅ |
| `subst(term, index, replacement)` | `term_subst(term, index, replacement)` | ✅ |
| `instantiate(body, arg)` | `term_instantiate(body, arg)` | ✅ |
| `whnf(term)` | `reducer_whnf(r, term)` | ✅ |
| `is_def_eq(lhs, rhs)` | `reducer_is_def_eq(r, lhs, rhs)` | ✅ |
| `infer(term, ctx)` | `typechecker_infer(tc, term, ctx)` | ✅ |
| `check(term, expected, ctx)` | `typechecker_check(tc, term, expected, ctx)` | ✅ |
