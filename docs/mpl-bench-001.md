# MPL-Bench-001 Task Suite

状态：V0.4.0-alpha

目录：

- `bench/tasks/`：12 个任务 JSON；
- `bench/baselines/`：参考程序；
- `bench/candidates/`：三个已知正确候选，用于回归验证器；
- `bench/experiments/`：离线 harness 示例。

所有任务目标均由 Observation Registry 的机器可验证观察组成。任务不评价“好听”。

## 任务列表

| # | ID | 类别 | Baseline | 目标要点 |
| --- | --- | --- | --- | --- |
| 01 | `mpl-bench-001-basic-notes` | 基础音符与节奏 | `basic.mpl` | 4 notes；onset 0/1/4/1/2/3/4；C-D-E-F |
| 02 | `mpl-bench-001-asymmetric-meter` | 不对称拍号 | `meter.mpl` | 7/8；7 个八分音符；2+2+3 |
| 03 | `mpl-bench-001-multi-period` | 多周期关系 | `poly.mpl` | 3-cell 对 2-cell；12 个起音；规范化节奏保持 |
| 04 | `mpl-bench-001-theme-transform` | 主题变换 | `theme.mpl` | 3 音 cell + 上方二度答句 |
| 05 | `mpl-bench-001-harmony-driven` | 和声驱动生成 | `harmony.mpl` | 共享 H；10 个起音；Dm9/G7b9 排列 |
| 06 | `mpl-bench-001-chord-voicing` | 和弦排列 | `voicing.mpl` | Dm9 四声部 [50,53,57,60] |
| 07 | `mpl-bench-001-voice-leading` | 声部连接 | `voice-leading.mpl` | Dm9→G7 总移动最小；8 个起音 |
| 08 | `mpl-bench-001-rhythm-replacement` | 节奏替换 | `replacement.mpl` | 同和声；16 个八分起音 |
| 09 | `mpl-bench-001-harmony-replacement` | 和声替换 | `replacement.mpl` | 同节奏；Fmaj7/G7 排列 |
| 10 | `mpl-bench-001-local-edit` | 局部结构编辑 | `basic.mpl` | 仅 F4→G4；保持事件/起音/长度 |
| 11 | `mpl-bench-001-mix-automation` | 混音与自动化 | `mix.mpl` | 2 whole；4 事件；符号事件保持 |
| 12 | `mpl-bench-001-mastering-target` | 母带技术目标 | `mastering.mpl` | 母带后 sample/true peak、integrated LUFS（需可信 WAV） |

## 参考结果（未调用模型）

用 baseline 作为候选时：

| 任务 | 结果 | 说明 |
| --- | --- | --- |
| 01–07 | pass | 目标与 baseline 一致 |
| 08 | fail | baseline 是四分节奏，目标要求八分 |
| 09 | fail | baseline 是 Dm9/G7b9，目标要求 Fmaj7/G7 |
| 10 | fail | baseline 是 F4，目标要求 G4 |
| 11 | pass | 混音/自动化目标与 baseline 一致 |
| 12 | unverifiable（无 WAV）；提供 `mastering.mastered.wav` + `mastering.manifest.json` 时 pass | 母带目标需可信渲染 |

`bench/candidates/` 中三个候选在 08/09/10 上 **pass**，用于证明验证器能接受正确改动，
而不是只接受原样复制。

这些是**基础设施验证结果**，不是模型生成成功率。未调用模型，因此不报告模型成功率。

## 参数化与 Split

- `bench/templates.json`：12 个模板 ID、类别、固定种子；
- `bench/splits.json`：`dev` / `pilot` / `heldout` 三组；
- 每个 task JSON 记录 `template` 与 `seed`，实例文件在评测前冻结；
- held-out 任务参考答案不进入模型上下文；验证器只在本地读取；
- 种子用于复现实例生成；当前实例由冻结文件表示，不在运行时随机生成。

## 负面案例

验证器基础设施覆盖：

1. 编译通过但目标错误：`TaskVerifierRejectsUnchangedCandidateForEditTask`；
2. 目标满足但保持契约违反：`TaskVerifierRejectsPreserveViolation`；
3. 保持契约满足但目标未完成：`TaskVerifierRejectsUnchangedCandidateForEditTask`；
4. 非授权作用域修改：`FrameContractRejectsUnauthorizedClipEdit`、`TaskVerifierRejectsScopeNoneChange`；
5. 错误音频绑定：`RenderManifestBindsSourceProductionAndAudio`、`ObservationRejectsUntrustedAudioHash`；
6. 预算耗尽：`VoiceLeadBudgetIsExplicit`；
7. 不支持语义：flat 任务 05/06/08（`not-applicable`）；
8. 观察不可验证：`TaskVerifierReportsUnverifiableAudioWithoutWav`。

## 说明

- 任务 10 声明 frame，仅允许 clip symbolicEvents 变化；Flat 组对该任务为 not-applicable；
- 任务 08/09 的 preserve 含和声/节奏观察，Flat 组为 not-applicable；
- 不适用任务不进入共同成功率。
- `bench/tasks/13-function.json` 是 Learnability 扩展（函数定义），不计入
  MPL-Bench-001 的 12 类正式任务。

## 运行

```text
mpl_lab validate bench/tasks
mpl_lab observe bench/baselines/basic.mpl
mpl_lab verify bench/tasks/01-basic-notes.json bench/baselines/basic.mpl
mpl_lab verify bench/tasks/10-local-edit.json bench/candidates/local-edit.mpl
mpl_lab verify bench/tasks/12-mastering-target.json bench/baselines/mastering.mpl \
  --audio bench/reference/mastering.mastered.wav \
  --manifest bench/reference/mastering.manifest.json
```

## 任务编写规则

1. 每个任务至少一个目标观察；
2. 目标必须可由注册表计算；不可验证目标不能算通过；
3. preserve 用于防止“改对目标、破坏其他语义”；
4. baseline 必须可编译；
5. 不允许把模型/生成器声明写入 pass 条件；
6. 任务 ID 稳定，不随实现细节变化；
7. 音频目标必须显式标记需要 WAV。

## 未覆盖

- 完整 task/preserve DSL；
- 声明级 allowed scope；
- 通用约束求解；
- 自动生成任务；
- 主观听感评分。
