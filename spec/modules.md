# Module Responsibilities

## Pipeline Overview

```
Source Code (string)
       │
       ▼
┌─────────────┐
│    Lexer     │  lexer.py
└─────┬───────┘
      │ Token[]
      ▼
┌─────────────┐
│    Parser    │  parser.py
└─────┬───────┘
      │ Decl[] (Surface AST)
      ▼
┌─────────────┐
│  Elaborator  │  elaborator.py
└─────┬───────┘
      │ RDecl[] (Restricted) + CDecl[] (Core)
      ▼
┌─────────────┐
│ Type Checker │  typechecker.py
└─────┬───────┘
      │ TcGlobalContext
      ▼
┌─────────────┐
│   Reducer    │  reducer.py
└─────────────┘
```

## Module Details

### Lexer (`lexer.py`)

**Input**: Source code string

**Output**: `list[Token]`

**Responsibilities**:
- Split source into tokens
- Track line/column positions
- Handle comments (`#` to end of line)
- Detect lexical errors (unmatched delimiters, invalid characters)

### Parser (`parser.py`)

**Input**: `list[Token]`

**Output**: `list[Decl]` (Surface AST), `ParseArtifacts` (spans, name sites)

**Responsibilities**:
- Parse tokens into surface AST
- Record node spans for source location tracking
- Record name sites for IDE-like features
- Detect syntax errors

### Surface AST (`surface.py`)

Defines the surface syntax tree nodes:
- `Expr`: `AtomExpr`, `AppExpr`, `ArrowExpr`, `PiExpr`, `LambdaExpr`, `LetExpr`, `MatchExpr`, `CaseExpr`, `EqExpr`, `ProductExpr`
- `Decl`: `VarDecl`, `FunDecl`, `InductiveDecl`, `ProductDecl`, `AxiomDecl`, `ExampleDecl`, `EquationDecl`
- `Param`, `CtorDecl`, `FieldDecl`, `MatchBranch`, `CaseBranch`

### Elaborator (`elaborator.py`)

**Input**: `list[Decl]` (Surface)

**Output**: `ElaborationResult` containing:
- `list[RDecl]` (Restricted syntax)
- `list[CDecl]` (Core syntax)

**Responsibilities**:
- Resolve names and dot notation
- Generate fresh names for anonymous binders
- Elaborate inductive types into type constructors, data constructors, and eliminators
- Generate projections for product types
- Handle match/case expressions (motive, branches, induction hypotheses)
- Perform capture-avoiding substitution (`restricted.py`)

### Restricted Syntax (`restricted.py`)

Intermediate representation between surface and core:
- `RDefinition`, `RTypeCtorDecl`, `RDataCtorDecl`, `REliminatorDecl`
- Utilities: `fold_pi_expr`, `fold_lam_expr`, `split_pi_expr`, `substitute_expr`

### Core Syntax (`core.py`)

Final internal representation using de Bruijn indices:
- `CVar`, `CGlobal`, `CType`, `CPi`, `CLam`, `CApp`
- `CDefinition`, `CTypeCtorDecl`, `CDataCtorDecl`, `CEliminatorDecl`

### Core Operations (`core_ops.py`)

Low-level operations on core terms:
- `shift(term, amount, cutoff)`: Shift de Bruijn indices
- `subst(term, index, replacement)`: Substitute a variable
- `instantiate(body, arg)`: Beta-reduce (subst index 0)

### Type Checker (`typechecker.py`)

**Input**: `list[CDecl]` (Core)

**Output**: `TcGlobalContext`

**Responsibilities**:
- Check declarations against the type system
- Bidirectional type checking: `check(term, expected)` and `infer(term)`
- Convertibility checking via `Reducer`
- Build global context with types and values

### Reducer (`reducer.py`)

**Input**: `TcGlobalContext`, term

**Output**: Reduced term, or equality result

**Responsibilities**:
- WHNF (Weak Head Normal Form) reduction
- NF (Normal Form) reduction
- Definitional equality checking (`is_def_eq`)
- Three strategies: `greedy`, `whnf`, `whnfv2`

### Runtime (`runtime.py`)

Data structures for the type-checking context:
- `GlobalEntry`: name, type, value, kind
- `InductiveInfo`: inductive type metadata
- `ConstructorInfo`: constructor metadata
- `EliminatorInfo`: eliminator metadata
- `TcGlobalContext`: all of the above

### Pipeline (`pipeline.py`)

High-level API that orchestrates the full pipeline:
- `run_pipeline(source)`: tokenize → parse → elaborate → type-check
- `check_file(path)`: read file and run pipeline
- `normal_form(term, ctx)`: compute NF

### Pretty Printer (`pretty.py`)

Converts core terms back to human-readable strings.

### Proof Visualizer (`proof_visualizer.py`)

Generates proof trees from theorem declarations for visualization.