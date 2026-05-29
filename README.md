# TinyChecker

一个基于依赖类型论的教学型定理证明助手内核实现。

目标不是做一个理论完备、功能完整的证明助手，而是保留一条尽量短、尽量透明的内核主线：

1. `Type : Type`
2. 依赖函数类型 `Pi`
3. lambda 与应用
4. 归纳类型定义
5. 自动生成归纳类型消去子 `D.rec`
6. 通过消去子定义递归函数和证明

# 理论基础和设计
  位于Doc文档

# 实现规范
  位于Specific

# 代码
  位于Implementation

# 案例
  如Tree,Nat,List
  位于Example

# 测试
  位于Test

# 当前定位

一个尽量短小的教学型内核，最适合：

1. 看清楚 `surface -> core` 的 elaboration
2. 理解依赖函数类型和 de Bruijn 表示
3. 观察归纳类型自动生成的 `D.rec`
4. 通过 `match` 语法糖或直接 `D.rec` 来定义递归
5. 通过 GUI 和证明叙述理解证明的结构





把我们变成本文 ok
项目变成系统 ok
改章节标题
lambda -> λ
pi -> Π

