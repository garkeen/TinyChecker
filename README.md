Notice:This is just a toy-style prototype test, referencing Coq/Lean.

注意：这只是一个玩具形式的原型测试，参考coq/lean。

# Pind

**English** | [中文](#中文)

---

A dependently-typed language type checker and proof assistant for the **Pind** (Pi + Inductive) language, with both Python and C implementations.


## Project Structure

```
TinyChecker/
├── Python/            # Python implementation
│   ├── core.py        # Core term representation
│   ├── core_ops.py    # De Bruijn operations (shift, subst, instantiate)
│   ├── surface.py     # Surface syntax (AST)
│   ├── lexer.py       # Tokenizer
│   ├── parser.py      # Parser
│   ├── elaborator.py  # Elaboration (surface → restricted)
│   ├── restricted.py  # Restricted expressions → core terms
│   ├── reducer.py     # Reduction engine (beta, iota, delta)
│   ├── typechecker.py # Bidirectional type checker
│   ├── pipeline.py    # End-to-end pipeline
│   ├── pretty.py      # Pretty printer
│   ├── runtime.py     # Runtime global context
│   ├── errors.py      # Error types
│   ├── cli.py         # Command-line interface
│   ├── gui.py         # GUI (PySide6)
│   └── Test/          # Python tests
├── C/                 # C implementation (semantically aligned with Python)
│   ├── term.h/c       # Core term (de Bruijn)
│   ├── core_ops.h/c   # shift, subst, instantiate
│   ├── surface.h/c    # Surface AST
│   ├── lexer.h/c      # Tokenizer
│   ├── parser.h/c     # Parser
│   ├── elaborator.h/c # Elaboration
│   ├── restricted.h/c # Restricted → core conversion
│   ├── reducer.h/c    # Reduction engine
│   ├── typechecker.h/c# Type checker
│   ├── pipeline.h/c   # End-to-end pipeline
│   ├── runtime.h/c    # Global context
│   ├── context.h/c    # Typing context
│   ├── pretty.h/c     # Pretty printer
│   ├── cdecl.h/c      # Core declaration types
│   ├── Makefile        # Build system
│   └── Test/          # C tests
├── Example/           # Example Pind programs
│   ├── Nat.pind       # Natural numbers, arithmetic proofs
│   ├── List.pind      # Lists, length proofs
│   ├── Tree.pind      # Binary trees, Fin type
│   ├── Sort.pind      # Insertion sort, sorted proofs
│   ├── FirstOrderlogic.pind       # First-order logic
│   ├── FirstOrderlogic_advanced.pind # Advanced FOL
│   ├── PropositionLogic.pind      # Propositional logic
│   └── ExistsNested.pind          # Nested existentials
└── Manual/            # Language manual and specification
```

## Features

- Dependent function types (Pi types)
- Inductive types with parameters and indices
- Sum types (tagged unions)
- Product types (records with auto-generated projections)
- Pattern matching with induction hypotheses
- Equality types and propositional reasoning
- Bidirectional type checking
- Multiple conversion strategies (`greedy`, `whnf`, `whnfv2`)
- Proof tree visualization
- GUI with syntax highlighting (PySide6)

## Quick Start

### Python

```bash
# Type check a file
python -m Python.cli Example/Nat.pind

# Compute normal form
python -m Python.cli Example/Nat.pind --nf addComm

# Launch GUI
python -m Python.gui Example/Nat.pind
```

### C

```bash
cd C
make tests
./Test/test_pipeline
```

## Example

```pind
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

inductive Eq (A:Type): A -> A -> Type {
| refl: (x:A) -> Eq A x x
};

# Addition via eliminator (not recursive definition)
fun add (m:Nat) (n:Nat): Nat {
  (match m in Nat
   return Nat -> Nat with
   | zero => \(n:Nat) => n
   | succ k [rec] => \(n:Nat) => succ (rec n)
   end)
  n
};

# Proof by induction
theorem addZeroRight (n:Nat): [Nat](add n zero)==n {
  match n as k in Nat
  return [Nat](add k zero)==k with
  | zero => refl Nat zero
  | succ k [ih] => eqCongSucc (add k zero) k ih
  end
};
```

## Manual

See [Manual/](Manual/) for the Pind language manual:

| Document | Description |
|----------|-------------|
| [overview.md](Manual/overview.md) | System overview, core concepts, quick start |
| [syntax.md](Manual/syntax.md) | Complete syntax specification (EBNF) |
| [types.md](Manual/types.md) | Type system, reduction rules, definitional equality |
| [modules.md](Manual/modules.md) | Implementation architecture, module responsibilities |
| [api.md](Manual/api.md) | CLI and Python/C API reference |
| [errors.md](Manual/errors.md) | Error types and debugging guide |

---

# 中文

一个依赖类型检查器和证明辅助工具，用于 **Pind**（Pi + Inductive）语言，包含 Python 和 C 两种实现。

## 项目结构

```
TinyChecker/
├── Python/            # Python 实现
│   ├── *.py           # 核心模块
│   └── Test/          # Python 测试
├── C/                 # C 实现（与 Python 语义对齐）
│   ├── *.h/c          # 核心模块
│   ├── Makefile       # 构建系统
│   └── Test/          # C 测试
├── Example/           # Pind 示例程序
└── Manual/            # 语言手册和规范
```

## 特性

- 依赖函数类型（Pi 类型）
- 带参数和索引的归纳类型
- 和类型（标签联合）
- 积类型（自动生成投影的记录类型）
- 带归纳假设的模式匹配
- 等式类型和命题推理
- 双向类型检查
- 多种转换策略（`greedy`、`whnf`、`whnfv2`）
- 证明树可视化
- 带语法高亮的 GUI（PySide6）

## 快速开始

### Python

```bash
# 类型检查文件
python -m Python.cli Example/Nat.pind

# 计算范式
python -m Python.cli Example/Nat.pind --nf addComm

# 启动 GUI
python -m Python.gui Example/Nat.pind
```

### C

```bash
cd C
make tests
./Test/test_pipeline
```

## 示例

```pind
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

inductive Eq (A:Type): A -> A -> Type {
| refl: (x:A) -> Eq A x x
};

# 通过消去子定义加法（非递归定义）
fun add (m:Nat) (n:Nat): Nat {
  (match m in Nat
   return Nat -> Nat with
   | zero => \(n:Nat) => n
   | succ k [rec] => \(n:Nat) => succ (rec n)
   end)
  n
};

# 归纳证明
theorem addZeroRight (n:Nat): [Nat](add n zero)==n {
  match n as k in Nat
  return [Nat](add k zero)==k with
  | zero => refl Nat zero
  | succ k [ih] => eqCongSucc (add k zero) k ih
  end
};
```

## 手册

详见 [Manual/](Manual/) 目录下的 Pind 语言手册：

| 文档 | 描述 |
|------|------|
| [overview.md](Manual/overview.md) | 系统概述、核心概念、快速开始 |
| [syntax.md](Manual/syntax.md) | 完整语法规范（EBNF） |
| [types.md](Manual/types.md) | 类型系统、归约规则、定义相等性 |
| [modules.md](Manual/modules.md) | 实现架构、模块职责 |
| [api.md](Manual/api.md) | CLI 和 Python/C API 参考 |
| [errors.md](Manual/errors.md) | 错误类型和调试指南 |
