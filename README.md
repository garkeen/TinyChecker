# TinyChecker

基于依赖类型论的定理证明语言 **Pind (Pi+Inductive)** 的解释器实现。

目标不是做一个理论完备、功能完整的证明助手，而是保留一条尽量短、尽量透明的解释器主线：

```
┌─────────┐     ┌─────────┐     ┌────────────┐     ┌──────────────┐     ┌─────────┐
│  LEXER  │ ──► │ PARSER  │ ──► │ ELABORATOR │ ──► │ TYPE CHECK   │ ──► │ REDUCER │
└─────────┘     └─────────┘     └────────────┘     └──────────────┘     └─────────┘
```

## 语言特性

- **归纳类型**：定义递归数据结构（如 `Nat`、`List`、`Tree`）
- **积类型**：定义记录/元组类型
- **和类型**：定义带标签的联合类型
- **依赖函数**：`(x:A) -> B x` 形式的 Pi 类型
- **模式匹配**：自动编译为消去子（`rec` / `elim`）
- **等式类型**：`[A] x == y` 形式的命题相等

## 快速开始

```bash
# 运行示例
python -m Implementation.pipeline Example/Nat.txt

# 运行测试
python -m pytest Test/
```

## 示例

```pind
inductive Nat {
  | zero: Nat
  | succ: Nat -> Nat
};

fun add (m:Nat) (n:Nat): Nat {
  match m in Nat
  return Nat -> Nat with
  | zero => \(n:Nat) => n
  | succ k [rec] => \(n:Nat) => succ (rec n)
  end
  n
};

theorem addComm (m:Nat) (n:Nat): [Nat](add m n)==(add n m) {
  -- 归纳证明...
};
```

## 目录结构

| 目录 | 说明 |
|------|------|
| `Implementation/` | 解释器核心实现 |
| `Example/` | 示例程序（Nat, List, Tree 等） |
| `Test/` | 测试用例 |
| `Specific/` | 设计文档 |