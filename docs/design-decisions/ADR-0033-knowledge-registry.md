# ADR-0033：外部知识注册表与契约

状态：已采纳（V0.5.0-alpha）

## 背景

模型上下文需要可复用音乐知识，但不能把未验证的 Prompt/审美规则塞进编译器，也不能
重复实现已有 Core/Stdlib 计算。

## 决策

1. 知识单元是结构化 JSON 元数据，位于 `knowledge/`，不进入编译器核心。
2. 五种类型：executable / relation / verification / construction / practice。
3. practice 永不为硬约束。
4. 知识引用 Core/Stdlib/Observation，不重复实现算法。
5. 注册表计算 contentHash/resolvedHash，支持依赖闭包与循环/缺失诊断。
6. 契约检查复用 Observation Registry + Compiler；无法执行的声明标 Unverified/Unsupported。
7. 初始库 33 单元；31 verified、1 unverified、1 deferred。

## 后果

- 知识可版本化、可追踪、可验证；
- 前置条件多数仍是声明式，不能声称全部机器验证；
- 需要维护 catalog 与 Core/Stdlib 的引用一致性。
