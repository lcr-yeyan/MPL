# Benchmark Generalization Specification（V0.4.0-rc）

## 1. 层级

| 层级 | 含义 | 当前任务 |
| --- | --- | --- |
| G0 | 已知任务、新参数 | 01 basic、02 meter、03 multi-period、04 theme |
| G1 | 已知算法、新素材 | 05 harmony-driven、06 chord-voicing |
| G2 | 未知算法组合 | 07 voice-leading、08 rhythm-replacement、09 harmony-replacement |
| G3 | 未提供直接解法 | 10 local-edit、11 mix-automation、12 mastering |

映射文件：`bench/generalization.json`。

## 2. 防泄漏

- `bench/splits.json` 固定 dev/pilot/heldout；
- held-out 参考候选/音频只在本地验证器使用，不进入模型提示词、语言上下文或实验报告；
- 修复反馈对 held-out 使用 `redactExpected`，移除 expected 参考值与语义差分 before/after；
- 模型上下文只包含任务描述、语言/标准库、结构化诊断、候选自身失败观察名称/状态；
- 不把参考候选源代码、target 值或 baseline 源码放进 held-out prompt。

## 3. 可复现

- 每个任务有 template 与 seed；
- 实例文件冻结；`bench/freeze.json` 记录 SHA-256；
- 任务生成、程序生成、执行、验证分别记录；
- 不适用（not-applicable）任务单独报告，不混入共同成功率。

## 4. 可满足性

每个任务有 baseline/参考候选：

- 01–07、11 baseline；
- 08–10 `bench/candidates/`；
- 12 baseline + mastered WAV + manifest。

所有参考结果由独立验证器计算，不由生成器自报。

## 5. 限制

- 当前参数化是固定 seed 的冻结实例，不是运行时随机生成器；
- 任务数量有限，不代表完整音乐任务分布；
- 不提供艺术质量评分；
- 不提供正式统计确认性结论；pilot 仅用于基础设施与设计检查。
