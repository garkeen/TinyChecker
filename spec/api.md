# API Specification

## CLI

### Usage

```
python -m Codes.cli <path> [--nf <name>] [--conv <strategy>]
```

### Arguments

| Argument | Description |
|----------|-------------|
| `path` | Source file path (required) |
| `--nf <name>` | Print normal form of a global name |
| `--conv <strategy>` | Conversion strategy: `greedy`, `whnf` (default), `whnfv2` |

### Output

On success:
```
typecheck: OK
```

With `--nf`:
```
typecheck: OK
nf <name>: <normal form>
```

On error:
```
<error type>: <message>
```

### Exit Codes

- `0`: Success
- Non-zero: Error

## Python API

### Pipeline Functions

```python
from Codes import run_pipeline, run_pipeline_detailed, check_file, normal_form, normal_form_of_global
```

#### `run_pipeline`

```python
def run_pipeline(source: str, *, conv_strategy: ConvStrategy = "whnf") -> tuple:
    """
    Run the full pipeline on source string.
    
    Returns:
        (tokens, decls, core_decls, global_ctx)
    """
```

#### `run_pipeline_detailed`

```python
def run_pipeline_detailed(source: str, *, conv_strategy: ConvStrategy = "whnf") -> tuple:
    """
    Run the full pipeline, including restricted declarations.
    
    Returns:
        (tokens, decls, restricted_decls, core_decls, global_ctx)
    """
```

#### `check_file`

```python
def check_file(path: str | Path, *, conv_strategy: ConvStrategy = "whnf") -> tuple:
    """
    Read and type-check a file.
    
    Returns:
        (tokens, decls, core_decls, global_ctx)
    """
```

#### `normal_form`

```python
def normal_form(term, global_ctx) -> CTerm:
    """
    Compute normal form of a term.
    """
```

#### `normal_form_of_global`

```python
def normal_form_of_global(name: str, global_ctx) -> CTerm:
    """
    Compute normal form of a global definition by name.
    """
```

### Types

```python
ConvStrategy = Literal["greedy", "whnf", "whnfv2"]
```

### Elaboration Functions

```python
from Codes import elaborate, elaborate_result, elaborate_restricted

def elaborate(decls: list[Decl]) -> list[CDecl]:
    """Elaborate surface declarations to core."""

def elaborate_result(decls: list[Decl]) -> ElaborationResult:
    """Elaborate, returning both restricted and core."""

def elaborate_restricted(decls: list[Decl]) -> list[RDecl]:
    """Elaborate to restricted syntax only."""
```

### Tokenization

```python
from Codes import tokenize, Token

def tokenize(source: str) -> list[Token]:
    """Tokenize source string."""
```

### Parsing

```python
from Codes import parse

def parse(tokens: list[Token]) -> list[Decl]:
    """Parse tokens into surface declarations."""
```

### Type Checking

```python
from Codes import check_program

def check_program(decls: list[CDecl], *, conv_strategy: ConvStrategy = "whnf") -> TcGlobalContext:
    """Type-check core declarations."""
```