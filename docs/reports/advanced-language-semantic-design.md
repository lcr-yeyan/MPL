# Advanced Language Semantic Design（V0.5.0-beta）

状态：设计，不在本阶段实现。

## 1. 目标

提高**已有原语 + 组合机制**的音乐计算闭包，而不是增加与 SMA 任务同名的内建函数。

## 2. 通用集合能力

SMA-001 的两个失败任务共享缺口：

- 列表 `append`/`cons`/`fold`/`scan`；
- `filter`/`map`/`flatMap` 或等价推导；
- 有界累积状态。

设计候选：

```text
fold(xs, init, fn(acc, x) -> Acc) -> Acc
map(xs, fn(x) -> T) -> [T]
flatMap(xs, fn(x) -> [T]) -> [T]
```

约束：

- 不使用隐式全局状态；
- 不在实时音频线程执行无界计算；
- 列表上限、调用上限、深度上限仍为硬错误；
- 不为模型便利增加特殊语法。

## 3. 函数抽象

- 高阶函数需要**显式函数值类型**，不能只靠名字；
- 递归/循环必须证明有界，或由资源预算截断并明确报告；
- 不承诺图灵完备；
- 编译期函数与运行期音频函数分离。

本 beta 不实现高阶函数；先评估集合 fold 的语义与成本。

## 4. 契约

- 语言级 `requires`/`ensures` 不做；保持外部 Knowledge/Stdlib Contract；
- 未来可引入类型化 effect/允许影响，但不得把审美规则变成合法性约束；
- 前置条件应在可执行契约中优先机器化。

## 5. Tie / 演奏连续性

- 从“边界 tie”升级为显式 voice/continuation 身份；
- 需要 performance event 层（logical note -> note-on/off/continuation）；
- 支持跨 clip 的连接时必须保留 voice id；
- 保持 MIDI/音频/自动化一致；
- 不与普通同音高自动合并混淆。

## 6. 音乐领域原语

候选（按通用性排序）：

1. `pitch list` 反射/提取（从 Fragment 获取有序音高/时间）；
2. 音程序列重建（fold 的自然用例）；
3. 音级过滤（filter/map）；
4. Section 一等值；
5. 完整 Tempo Map；
6. 区间级关系。

## 7. 判定标准

只有当多个 SMA 任务因同一通用能力失败时，才增加该能力；专用函数只作为 Stdlib 派生实现。
