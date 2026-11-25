# Task Requirement Specification（V0.5.0-alpha）

## 1. 输入

只使用已有结构化 Task Contract：

- target observations；
- preserve observations；
- frame allow/deny；
- validity；
- budget；
- baseline source（用于 capability 推断）。

不做自由自然语言意图理解。

## 2. 输出

`TaskRequirementSet`：

| 字段 | 来源 |
| --- | --- |
| targetObservations | Task target names |
| preserveObservations | Task preserve names |
| frameObjects / frameProperties | frame rules |
| allowedEffects | frame allow `object.property` |
| requiredTypes | Observation Registry input types |
| requiredCapabilities | baseline source 中出现的 core/stdlib/module 能力 |
| unknown | 未知 observation / 无法推断 |
| unsupported | frame 覆盖范围外对象/属性 |
| budget | TaskBudget |

## 3. 规则

- 未知 observation → `unknown`，不猜测；
- frame 对象/属性不在 `frameCoverage()` → `unsupported`；
- stdlib 只在 baseline 实际 import 时作为 capability；
- 不因为任务标题包含“和声”就自动推断和声需求；
- 不自动补充用户未授权的音乐决策。

## 4. 限制

- 不解析自然语言任务描述；
- capability 由文本扫描推断，可能漏掉间接使用；
- 类型映射是静态表，新观察需更新。

## 5. 测试

- 基础任务 target/preserve/frame 解析；
- 未知观察；
- frame 覆盖外属性；
- 确定性排序。
