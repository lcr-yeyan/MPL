# ADR-0002：保留两级音乐 IR

状态：已采纳（V0.0.1 引入，V0.0.2 复核）

## 背景

直接把源码或 AST 编译为 MIDI 会丢失动机、变奏、和声与段落之间的生成关系，使增量编译、结构变换和可解释性难以实现。

## 决策

1. **Music IR**：类型化依赖 DAG，保留生成关系与上下文依赖。
2. **Production IR**：时间线、轨道、片段、资产绑定，面向 DAW/音频后端。
3. 两者之间通过显式 lowering 连接，Production IR 记录 `sourceObject`。
4. 不在 V0.0.2 引入 LLVM/MLIR 或通用 Pass 基础设施，只保留轻量 PassManager。

## 后果

- 修改动机后，可以沿依赖图失效并重算受影响对象。
- Production IR 可以被不同后端消费，而不暴露创作图。
- 限制：Production IR 的 clip 是音符快照；跨层增量编译尚未实现。
