# Error Handling

## Error Hierarchy

All errors inherit from `TinyCheckerError`:

```
TinyCheckerError
├── LexError
├── ParseError
├── ElabError
├── TypeCheckError
└── ReducerError
```

## Error Types

### LexError

Raised during tokenization.

| Message Pattern | Cause |
|-----------------|-------|
| `unexpected character 'x'` | Invalid character in source |
| `unmatched ')'\|']'\|'>'` | Closing delimiter without matching opener |
| `unbalanced delimiters` | EOF with unclosed delimiters |
| `invalid dot identifier at line:col` | Dot not followed by letter/underscore |
| `only one dot is allowed in DOT identifiers` | Multiple dots in identifier |

### ParseError

Raised during parsing.

| Message Pattern | Cause |
|-----------------|-------|
| `expected TOKEN, got TOKEN at line:col` | Unexpected token |
| `unexpected token KIND at line:col` | Token not valid in context |
| `'_' is not allowed as an expression` | Underscore used as standalone expression |
| `invalid expression at line:col` | Cannot parse expression |
| `match family head must be an atom` | Non-identifier in match family position |
| `equation name mismatch` | Equation declared name differs from definition |

### ElabError

Raised during elaboration.

| Message Pattern | Cause |
|-----------------|-------|
| `duplicate global name: NAME` | Name already defined |
| `unbound name: NAME` | Reference to undefined name |
| `NAME arity must end in Type` | Inductive type not returning Type |
| `sum declarations cannot have parameters` | Sum type with parameters |
| `constructor NAME must return OWNER` | Constructor doesn't return its type |
| `constructor NAME has wrong family arity` | Wrong number of type arguments |
| `constructor NAME must use uniform parameters` | Parameters not uniform |
| `recursive field of NAME has wrong family arity` | Recursive field mismatch |
| `sum/product constructor field cannot mention NAME` | Self-reference in non-inductive |
| `case can only eliminate sum/product types` | Case on non-sum/product |
| `match can only eliminate inductive types` | Match on non-inductive |

### TypeCheckError

Raised during type checking.

| Message Pattern | Cause |
|-----------------|-------|
| `duplicate global name: NAME` | Name already in context |
| `unbound de Bruijn variable: INDEX` | Invalid variable index |
| `unbound global variable: NAME` | Reference to unknown global |
| `non-function application: TYPE` | Applying non-function |
| `lambda expected Pi type, got TYPE` | Lambda against non-Pi type |
| `type mismatch: EXPECTED vs ACTUAL` | Types not definitionally equal |
| `cannot infer type of TERM` | Cannot synthesize type |

### ReducerError

Raised during reduction.

| Message Pattern | Cause |
|-----------------|-------|
| `unknown global NAME` | Reference to undefined global |
| `WHNF exceeded step limit` | Reduction exceeds 200,000 steps |
| `greedy conversion exceeded step limit` | Conversion exceeds limit |
| `whnfv2 conversion exceeded step limit` | Conversion exceeds limit |
| `unknown term in NF: TERM` | Unexpected term in normalization |

## Error Reporting

Errors include location information when available:
- `LexError`: `at {row}:{col}`
- `ParseError`: `at {row}:{col}`

The pipeline catches all `TinyCheckerError` subtypes and returns them as error results rather than propagating exceptions.