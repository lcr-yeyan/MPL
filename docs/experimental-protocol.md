# Experimental Protocol（V0.4.0-rc）

状态：冻结草案。正式评价前不得修改任务目标、验证规则、观察语义与主要指标。

## 1. 研究因素

三因素：

1. **表示机制**：Flat Event JSON vs MPL 程序；
2. **标准库**：MPL Core vs MPL + Standard Library；
3. **验证反馈**：无反馈 vs 有界语义反馈。

六组：

| Arm | 表示 | 标准库 | 反馈 |
| --- | --- | --- | --- |
| A0 | Flat Event JSON | — | 无 |
| A1 | Flat Event JSON | — | 有 |
| B0 | MPL Core | 否 | 无 |
| B1 | MPL Core | 否 | 有 |
| C0 | MPL + stdlib | 是 | 无 |
| C1 | MPL + stdlib | 是 | 有 |

A0/A1 不能使用 MPL 生成来源、和声时间线、模块或声部搜索；这些都是表示能力差异，不是模型失败。
B0/B1 不得直接或间接使用 `Std*` 模块（编译后模块闭包检查）。

## 2. 任务契约

- 共同任务集合：`bench/tasks/` 中明确标记适用于该 arm 的任务；
- 每个任务有 target / preserve / frame / validity；
- 不适用任务返回 `not-applicable`，从共同成功率中排除；
- 所有 arm 使用同一 task JSON、baseline、观察定义、tolerance、预算与最终验证器；
- 每个任务实例有 template/seed，实例文件冻结。

## 3. 能力矩阵

| 任务 | A0/A1 | B0/B1 | C0/C1 |
| --- | --- | --- | --- |
| 01 basic | 适用 | 适用 | 适用 |
| 02 7/8 | 适用 | 适用 | 适用 |
| 03 multi-period | 适用 | 适用 | 适用 |
| 04 theme | 适用 | 适用 | 适用 |
| 05 harmony-driven | 不适用（无和声来源） | 适用 | 适用 |
| 06 chord-voicing | 不适用（无 clip range） | 适用 | 适用 |
| 07 voice-leading | 适用（事件序列） | 适用 | 适用 |
| 08 rhythm replacement | 不适用（preserve 含和声） | 适用 | 适用 |
| 09 harmony replacement | 适用 | 适用 | 适用 |
| 10 local edit | 不适用（frame 需生产） | 适用 | 适用 |
| 11 mix automation | 不适用（frame 需生产） | 适用 | 适用 |
| 12 mastering | 不适用（Harness 未绑定 Flat 音频） | 适用 | 适用 |

A 组共同集合只含 01–04、07、09；其他任务不进入 A 组成功率。

## 4. 反馈

- 仅提供结构化诊断、失败观察名称/状态/原因、语义差分；
- 不提供 held-out 参考解、expected 值（held-out 使用 redaction）、参考音频；
- 不放宽 target/preserve/frame/tolerance；
- 反馈轮数受 `RepairBudget` 限制。

## 5. 评价指标

- First-pass success；
- Final task success；
- Target/Preserve/Frame/Validity 分项；
- Unverifiable / infrastructure error；
- Repair success 与 regression；
- 总输入/输出/缓存 Token；
- 端到端延迟；
- 每个成功任务成本。

## 6. 配对与统计

- 同一任务实例跨 arm 配对；
- 报告逐任务、逐模型、逐 arm 原始结果；
- 配对差异使用确定性 bootstrap 95% CI；
- 同一任务重复生成按实例分别记录；
- pilot 仅用于设计/基础设施，不作确认性结论。

## 7. 版本冻结

| 项 | 版本 |
| --- | --- |
| Compiler | 0.4.0-rc |
| Language | 0.3.0 |
| Stdlib | 0.3.0 |
| Benchmark | 0.4.0-rc |
| Observation | 1.0.0 |
| Protocol | 0.4.0-rc |

任务集合、baseline、参考产物与 SHA-256 记录于 `bench/freeze.json`。
