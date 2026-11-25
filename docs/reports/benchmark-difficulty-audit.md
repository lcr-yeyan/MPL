# Benchmark Difficulty Audit（V0.5.0-alpha）

## 1. 分层

| Level | 定义 | 任务 | Split | 目标暴露 |
| --- | --- | --- | --- | --- |
| D0 | 精确规格转录 | 01 basic、02 meter | dev | 完整规格；仅测合法性/基础能力 |
| D1 | 音乐算法构造 | 04 theme、13 function | dev | 约束，不暴露完整参考序列 |
| D2 | 关系/约束推理 | 06 voicing、07 voice-leading | pilot | 约束，独立验证 |
| D3 | 选择性编辑 | 08 rhythm replacement、10 local edit | heldout | 编辑/保持契约，不暴露目标音符 |
| D4 | 多阶段音乐生产 | 11 mix、12 mastering | heldout | 技术目标/preserve/frame，不暴露参考音频 |

文件：`bench/v0.5/difficulty.json`。

## 2. 能力矩阵

| Arm | D0 | D1 | D2 | D3 | D4 |
| --- | --- | --- | --- | --- | --- |
| A0/A1 | 是 | 是 | 否 | 否 | 否 |
| B0/B1 | 是 | 是 | 是 | 是 | 是 |
| C0/C1 | 是 | 是 | 是 | 是 | 是 |

Flat 不提供和声/声部生成来源、frame 生产语义或可信音频绑定，因此 D2–D4 不进入 A 组共同任务。

## 3. 不泄漏参考解

- D0 可以给精确目标；仅用于基础/合法性。
- D1–D4 的 V0.5 候选 prompt 只给约束、保持/作用域条件和输出格式；
- 不把完整参考音符序列、完整目标程序或参考音频放入 prompt；
- held-out 不使用参考候选/参考 WAV 构建上下文；
- V0.4 的 pilot prompt 曾给出精确 target，属于 D0 风格，已作为 ceiling 记录，不用于 V0.5 结论。

## 4. 冻结

V0.4 的 13 个 task JSON 与结果不覆盖；V0.5 候选规范独立在 `bench/v0.5/`。
正式实验前应冻结 task/prompt/context 哈希。

## 5. 限制

- 只有能力矩阵与任务规范，没有 V0.5 确认性模型结果；
- D2–D4 的 prompt 尚未经过模型可解性验证；
- 难度分级是设计分类，不是统计难度估计；
- 不提供艺术质量评分。
