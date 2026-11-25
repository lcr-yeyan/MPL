# Music Knowledge Specification（V0.5.0-alpha）

## 1. 定位

音乐知识单元是**独立于编译器核心**的类型化、可验证元数据。它引用现有 MPL Core、
Stdlib、Observation Registry 和验证基础设施，不重复实现音乐计算，也不把自然语言当作
核心语义。

## 2. 知识类型

| kind | 含义 | 是否可成为硬约束 |
| --- | --- | --- |
| `executable` | 可执行计算/算子 | 是（契约检查通过时） |
| `relation` | 声明式音乐关系 | 是 |
| `verification` | 验证/分析 | 是 |
| `construction` | 可复用程序构造 | 是 |
| `practice` | 非强制音乐实践建议 | **否** |

`practice` 知识永远不能自动成为 target/preserve/frame 的硬约束。

## 3. 字段

每个知识单元具有：

`knowledgeId`、`version`、`category`、`kind`、`referenceKind`、`description`、`inputTypes`、
`outputTypes`、`parameters`、`preconditions`、`postconditions`、`invariants`、
`allowedEffects`、`requiredContext`、`dependencies`、`implementationRef`、
`observationRefs`、`examples`、`checks`、`source`、`license`、
`verificationStatus`、`unsupportedReason`、`tags`、`contentHash`、`resolvedHash`。

`referenceKind` 是实现载体（callable/syntax/stdlib/observation/task/benchmark/deferred），与语义 `kind` 独立。

`contentHash` / `resolvedHash` 由注册表计算，不能手写。

## 4. 契约表达

- `preconditions` / `postconditions` / `invariants` 是声明式字符串；
- `checks` 是机器可检查的观察断言：

```json
{"phase":"postcondition","program":"bench/baselines/basic.mpl",
 "observation":"event_count","comparator":"equals","expected":4}
```

- comparator：`equals` / `gte` / `lte` / `tolerance`；
- 无法执行的声明必须由 `verificationStatus=unverified` 标记；
- 不支持的能力标 `unsupported` / `deferred`，不得假装实现。

## 5. 初始知识库

`knowledge/v1/catalog.json`，33 个单元：

| 类别 | 数量 |
| --- | --- |
| Rhythm | 6 |
| Melody | 5 |
| Harmony | 5 |
| Voicing | 6 |
| Arrangement | 6 |
| Production | 5 |

类型：executable 15、construction 11、relation 3、verification 3、practice 1。

## 6. 验证覆盖

- 33/33 个示例程序通过冻结编译器；
- 31/33 个单元契约验证为 pass；
- 1 个 `deferred`（motivic development，没有通用算法）；
- 1 个 `unverified`（practice voice-leading preference，advisory）；
- 36 个机器可检查检查（postcondition/invariant）；
- 前置条件目前主要是声明式，未全部机器执行；报告明确区分。
- 实现引用包含 `core:` 19、`syntax:` 7、`stdlib:` 3、`observation:` 1、`task:` 1、`benchmark:` 1、`deferred:` 1；31 verified 是示例级契约检查，不等于 31 个独立可调用符号。

## 7. 禁止

- 把 Prompt 文本当作已实现知识；
- 把 practice 建议自动变成硬约束；
- 在知识库中实现与 Core/Stdlib 重复的音乐算法；
- 把未验证声明标为 verified；
- 把 held-out 参考解写入知识示例。

## 8. 来源与许可

初始库来自 V0.4.0 Final 的 Core、Stdlib、Benchmark 与规范；许可证为 MPL project。
外部音乐理论条目必须记录来源与适用范围，不得直接成为语言合法性约束。
