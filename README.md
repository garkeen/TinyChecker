# TinyChecker

**English** | [中文](#中文)

---

A dependently-typed language type checker and proof assistant for the **Pind** (Pi + Inductive) language.

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

```bash
# Type check a file
python -m Codes.cli Example/Nat.pind

# Compute normal form
python -m Codes.cli Example/Nat.pind --nf addComm

# Launch GUI
python -m Codes.gui Example/Nat.pind
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

## Specification

See [Spec/](Spec/) for the Pind language specification:

| Document | Description |
|----------|-------------|
| [overview.md](Spec/overview.md) | System goals, supported features, limitations |
| [syntax.md](Spec/syntax.md) | Lexical and grammatical specification (EBNF) |
| [types.md](Spec/types.md) | Type system, reduction rules, definitional equality |
| [modules.md](Spec/modules.md) | Module responsibilities and data flow |
| [api.md](Spec/api.md) | CLI and Python API interface |
| [errors.md](Spec/errors.md) | Error types and reporting |

---

# 中文

一个依赖类型检查器和证明辅助工具，用于 **Pind**（Pi + Inductive）语言。

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

```bash
# 类型检查文件
python -m Codes.cli Example/Nat.pind

# 计算范式
python -m Codes.cli Example/Nat.pind --nf addComm

# 启动 GUI
python -m Codes.gui Example/Nat.pind
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

## 规范文档

详见 [Spec/](Spec/) 目录下的 Pind 语言规范：

| 文档 | 描述 |
|------|------|
| [overview.md](Spec/overview.md) | 系统目标、支持的特性、限制 |
| [syntax.md](Spec/syntax.md) | 词法和语法规范（EBNF） |
| [types.md](Spec/types.md) | 类型系统、归约规则、定义相等性 |
| [modules.md](Spec/modules.md) | 模块职责和数据流 |
| [api.md](Spec/api.md) | CLI 和 Python API 接口 |
| [errors.md](Spec/errors.md) | 错误类型和报告 |
