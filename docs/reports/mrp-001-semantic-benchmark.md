# MRP-001 Semantic Benchmark（V0.3.0-rc）

日期：2026-11-25

工程：`examples/math-rock.mpl`（Math Rock Reference Project）

本报告把“语义正确性”与“艺术评价”分开：每项只验证结构、时间、计数、映射、约束状态与
诊断，不判断作品是否好听、是否有风格或是否“足够复杂”。

## 1. 复现方式

```text
# 全量测试（Debug / Release 各 277 项）
build\Debug\mpl_tests.exe
build\Release\mpl_tests.exe

# MRP-001 产物
build\Release\mplc.exe build examples/math-rock.mpl -o build/math-rock --tail 1.0

# 音频分析
build\Release\mplc.exe analyze build\math-rock\math-rock.wav
```

自动验证位于 `tests/test_benchmark.cpp`（23 项）以及 `tests/test_meter.cpp`、
`tests/test_rhythm.cpp`、`tests/test_harmony.cpp`、`tests/test_modules.cpp`、
`tests/test_relations.cpp`、`tests/test_functions.cpp`。

## 2. 产物概览

| 项 | 值 |
| --- | --- |
| Music IR objects | 144 |
| Rhythm objects | ≥ 10（literal/realign/union/intersect/accent 等） |
| Harmony objects | 2（共享 H 与替换 HAlt） |
| Modules | 4（root + StdRhythm/StdHarmony/StdVoicing） |
| Production | 4 tracks, 14 clips, 247 notes |
| 段落 | A: 4 bars 7/8；B: 4 bars 5/8；Reprise: 4 bars 4/4 |
| 逻辑长度 | 10 whole notes |
| 时长 | 18.18 s（不含 1.0 s 尾音） |
| MIDI 拍号事件 | tick 0 / 6720 / 11520 |
| WAV | 48 kHz stereo float32，921,818 frames |
| Sample Peak | -3.64 dBFS |
| True Peak | -3.26 dBTP |
| Integrated | -15.69 LUFS |
| Short-term max | -14.61 LUFS |
| Function definitions | 21（含标准库） |
| Function calls | 63（cache hits 0） |

产物：

- `build/math-rock/math-rock.mid`
- `build/math-rock/math-rock.wav`
- `build/math-rock/math-rock.music.ir`
- `build/math-rock/math-rock.production.ir`
- `build/math-rock/math-rock.temporal.ir`
- `build/math-rock/math-rock.trace.json`

## 3. 需求逐项验证

### SB-01 变拍号 7/8、5/8、4/4

- 自然语言指令：作品在 A 段使用 7/8，B 段使用 5/8，Reprise 使用 4/4；每段 4 小节。
- MPL 实现：

  ```text
  section A(4 bars) meter 7/8 grouping 2+2+3 { ... }
  section B(4 bars) meter 5/8 grouping 2+3 { ... }
  section Reprise(4 bars) meter 4/4 grouping 2+2 { ... }
  ```

- 明确预期：全局 Meter Map 三个事件，位置 0、7/2、6；小节号 0、4、8；逻辑长度 10。
- 实际：`test_benchmark.cpp::BenchmarkMeterMapMatchesMrpPlan`；Production IR `meterMap` 与预期一致。
- 自动验证：`BenchmarkMeterMapMatchesMrpPlan`。
- 不支持/诊断：拍中变拍号 → `meter-change-invalid`（`BenchmarkUnsupportedMidBarMeterIsDiagnosed`）。

### SB-02 非均匀节拍分组

- 指令：7/8 使用 2+2+3；5/8 使用 2+3；4/4 使用 2+2。
- 实现：Section 头 `grouping 2+2+3` 等。
- 预期：grouping 总和等于分子；层级 level 在小节/组/拍/细分处为 3/2/1/0；7/8 的 2/8 与 4/8 是组起点。
- 实际：`BenchmarkNonUniformGroupingAndHierarchy` 通过；7/8=3+2+2 时 3/8 与 5/8 变为组起点。
- 自动验证：`BenchmarkNonUniformGroupingAndHierarchy`、`MeterMapMetricHierarchyAlternateGrouping`。
- 不支持：拍中变拍号；grouping 不直接改变力度。

### SB-03 独立 Rhythm 对象

- 指令：节奏必须能独立于音高、乐器、MIDI 通道定义与复用。
- 实现：`let R3 = rhythm { hit 1/8 accent 2, ... };` 等。
- 预期：Rhythm 节点包含逻辑长度、onset、duration、accent、rest、origin；不只是 Fragment 的 offset 集合。
- 实际：Music IR 中 `rhythm-literal`、`rhythm-realign`、`rhythm-union`、`rhythm-intersect`、`rhythm-accent-by-meter` 等节点；R3 逻辑长度 3/8、3 个起音。
- 自动验证：`BenchmarkIndependentRhythmObjects`、`RhythmLiteralEventsRestsAndAccents`。
- 不支持：Rhythm 不含音高；与音高组合必须显式 zip/cycle。

### SB-04 两个不同长度的节奏周期

- 指令：同时使用 3/8 与 7/8 周期，并让它们精确对齐。
- 实现：`let Cycle = commonPeriod(R3, R7);`，`CellCycle = realign(R3, Cycle)`，`BarCycle = realign(R7, Cycle)`。
- 预期：共同周期 21/8；CellCycle 7 份（21 个起音）；BarCycle 3 份（18 个起音）。
- 实际：与预期一致；`CompositeCycle = union(CellCycle, BarCycle, 1/16)` 逻辑长度 21/8、21 个起音。
- 自动验证：`BenchmarkTwoCycleLengthsAndCommonPeriod`、`BenchmarkExactRealignment`、`BenchmarkCompositeOnsetUnion`。
- 不支持：无界 LCM；超过 1e9 报 `rhythm-period-overflow`。

### SB-05 精确周期重新对齐

- 指令：把 3/8 与 7/8 精确扩展到共同边界，而不是近似循环。
- 实现：`realign(rhythm, targetPeriod)`，target 必须是周期整数倍。
- 预期：对齐后逻辑长度 21/8，内容末端不超过 21/8；非整数倍报 `rhythm-realign`。
- 实际：CellCycle/BarCycle 均为 21/8；`RhythmRealignRepeatsToTargetPeriod` 断言 12 个事件与 `1/2` 目标拒绝。
- 自动验证：`BenchmarkExactRealignment`、`RhythmRealignRepeatsToTargetPeriod`。

### SB-06 节奏与音高组合

- 指令：用显式音高序列为 Rhythm 配器；休止不消耗音高；数量不匹配必须诊断。
- 实现：`zip(R, pitches, velocity)` 严格；`cycle(R, pitches, velocity)` 循环填充。
- 预期：Guitar A 24 notes（4 个 7/8 小节 × R7 6 起音）；Guitar B 16 notes（4 × 4）；accent 2 → velocity base+16。
- 实际：与预期一致；每个 note 带 `rhythmOrigin` 与 `rhythmEventIndex`。
- 自动验证：`BenchmarkRhythmPitchCombinationCounts`、`ZipMapsAccentsToVelocity`、`CycleFillsPitchesAndRestsConsumeNone`。
- 不支持/诊断：严格 zip 数量不匹配 → `rhythm-pitch-count`；负 onset → `rhythm-pitch`；空音高列表 → `rhythm-pitch`。

### SB-07 用户定义函数

- 指令：用函数复用主题单元、鼓型与 lead 生成，而不是手写全部音符。
- 实现：`cell`、`counter`、`lead`、`drums7`、`drums5`、`drums4`。
- 预期：21 个函数定义（MRP 6 + 标准库 15）；调用 Trace 记录 function/instance/depth/args/context/metricContext；函数可嵌套。
- 实际：21 个定义、63 次调用；drums*/voicedBarAt/leadBarAt 等记录 section 级 metricContext。
- 自动验证：`BenchmarkUserFunctionsAndCalls`、`FunctionNestedCallsAndTrace`。
- 不支持：递归、提前 return、模块、函数值。

### SB-08 主题再现

- 指令：A 段主题在 Reprise 原样/变体返回。
- 实现：`let Theme = cycle(R3, [D4, F4, A4], 92);`；A 段使用变体 `VerseTheme = diatonicTranspose(Theme, round(Variant))`，Reprise 使用 `Theme` 原形。
- 预期：A 段首三音 E4/G4/Bb4；Reprise 首三音 D4/F4/A4；两处 onset 相同（0、1/8、1/4）。
- 实际：与预期一致。
- 自动验证：`BenchmarkThemeRecapitulation`。
- 说明：这是语义再现（同一节奏细胞与音高关系），不是审美评价。

### SB-09 局部参数变奏

- 指令：修改 `Variant`/`Seed` 产生不同版本，同时保持确定性。
- 实现：`Variant` 影响 `round(Variant)` 的变体移调；`Seed` 影响 lead 的随机选择。
- 预期：同参数两次编译 MIDI 相同；不同 Variant/Seed 产生不同 MIDI。
- 实际：`MathRockReferenceProjectVariants` 与 `FunctionControlFlowAndRandom` 通过。
- 自动验证：`MathRockReferenceProjectVariants`、`FunctionControlFlowAndRandom`。

### SB-10 基础跨声部节奏关系

- 指令：两个声部的起音集合可求交、并、差、补，并可让一个声部跟随另一个声部的重音。
- 实现：`onsetSet`、`intersect`、`union`、`difference`、`complement`、`accentFollow`；B 段鼓使用 `accentFollow(Displaced5, R5, 1/8)`。
- 预期：关系作用于起音集合，不作用于完整持续区间；结果逻辑长度为共同周期。
- 实际：`CompositeCycle` 为 CellCycle/BarCycle 的并集；`CrossVoiceRhythmRelationInProject` 得到 1 个共同起音；`accentFollow` 精确/网格测试通过。
- 自动验证：`BenchmarkCompositeOnsetUnion`、`CrossVoiceRhythmRelationInProject`、`AccentFollowCopiesMatchingAccents`、`AccentFollowGridBuckets`。
- 不支持：通用约束求解、声部进行/平行五八度检查、区间吸收语义。

### SB-11 MIDI 与 WAV 时间映射

- 指令：MIDI 输出每个拍号事件；WAV 在变拍号下保持绝对时间正确。
- 实现：`ProductionIR::meterMap` → MIDI `FF 58`；WAV 由 Reference Audio Backend 按绝对 MusicalTime 渲染。
- 预期：拍号 tick 0/6720/11520；B 段起点 7/2 whole 对应 sample 50909（132 BPM、8 kHz 测试配置）；音频内容末端 9.875 whole。
- 实际：与预期一致；正式 WAV 48 kHz、921,818 frames。
- 自动验证：`BenchmarkMidiMeterTimeline`、`BenchmarkAudioTimeline`、`MidiExportsEveryMeterEvent`。
- 不支持：MIDI 标准无 grouping 元事件；grouping 保留在 Production/Trace。

### SB-12 函数身份、缓存与独立实例

- 指令：共享 Music Value 不合并调用身份；独立演奏实例拥有独立表情；改变函数定义会使相关结果变化；无关上下文变化不改变结果。
- 实现：缓存键加入函数版本、Key、`mc<mapId>@<anchor>` 与实参；Clip 仍独立 `InstanceId`。
- 预期：
  - 同参同上下文调用命中并共享对象；
  - 不同 Section 的同一函数因 metricContext 不同而不共享；
  - 同一 Music Value 的两个 Clip 可有不同 `PerformanceExpression`；
  - 改变 inner 函数会改变 outer 的输出；
  - 改变 Section B 的拍号不改变 Section A 的同步结果。
- 实际：全部通过。
- 自动验证：`FunctionCallIdentityAndCacheSharing`、`SectionMeterSeparatesFunctionCacheContexts`、`FunctionSharedValueIndependentPerformanceInstance`、`FunctionTransitiveDefinitionChange`、`FunctionContextIsolationAcrossSections`。
- 不支持：跨编译持久缓存；完整随机流派生/算法版本契约仅为设计约定。

### SB-13 不支持能力的诊断

| 能力 | 诊断 | 验证 |
| --- | --- | --- |
| 拍中变拍号 | `meter-change-invalid` | `BenchmarkUnsupportedMidBarMeterIsDiagnosed` |
| Section 长度不对齐小节 | `section-not-bar-aligned` | `SectionDurationMustBeBarAligned` |
| bars 锚点不在小节起点 | `meter-metric-duration` | `MeterMapResolvesBarsAtBarBoundary` |
| 周期 LCM 溢出 | `rhythm-period-overflow` | `RelationPeriodOverflowIsRejected` |
| zip 数量不匹配 | `rhythm-pitch-count` | `BenchmarkStrictZipMismatchIsDiagnosed` |
| 负 onset 组合 | `rhythm-pitch` | `NegativeRhythmCombineIsDiagnosed` |
| 事件数超限 | `rhythm-event-limit` | `RhythmEventLimitIsEnforced` |
| 非整数倍 realign | `rhythm-realign` | `RhythmRealignRepeatsToTargetPeriod` |

### SB-14 Temporal IR 的绝对时间映射

- 指令：Temporal IR 使用绝对 MusicalTime，不随 Meter Map 改变时间刻度。
- 实现：`curve Auto on musical for 4 whole = linear(0, 1);`，在两个不同拍号的 Section 上绑定。
- 预期：曲线在 0、2、7/4 whole 处分别为 0、0.5、0.4375；拍号变化不重缩放曲线。
- 实际：与预期一致。
- 自动验证：`BenchmarkTemporalIrUsesAbsoluteTime`。
- 不支持：音频速率曲线调制；Physical/Sample 域求值仍保留给音频运行时。

### SB-15 随机算法版本契约

- 指令：随机原语必须可复现，并在 Trace 中记录算法与版本，便于跨版本比较。
- 实现：`random(seed, index)`（splitmix64）；Trace 的 `random` 字段记录 algorithm/version/primitive。
- 预期：Trace 包含 `splitmix64`、版本 1 与 `random(seed,index)`。
- 实际：与预期一致；`docs/random-stream-contract.md` 定义局部随机流的派生契约（API 未实现）。
- 自动验证：`BenchmarkRandomContractIsRecorded`、`FunctionControlFlowAndRandom`。
- 不支持：`randomStream` API、跨编辑稳定随机流、算法升级迁移工具。

### SB-16 PitchClass、ChordSymbol 与扩展和弦

- 指令：和弦必须能表达根音、质量、扩展音、变化音与 slash bass，并支持 pitch-class 查询。
- 实现：`chord("Dm9")`、`chord("G7b9")`、`chord("A7#9")`、`chord("C/E")`。
- 预期：Dm9 含 D/F/A/C/E；G7b9 含 G/B/D/F/Ab；C/E 的 bass 为 E、inversion=1。
- 实际：与预期一致；`ChordParsesTriadsAndSevenths`、`ChordParsesExtensionsAndAlterations`、
  `ChordHandlesSlashBass`、`BenchmarkExtendedChordsAndInversions` 通过。
- 不支持：完整微分音/任意调律；自动功能标注。

### SB-17 Harmony Timeline 与跨段查询

- 指令：一条共享和声时间线覆盖全曲，并可在任意绝对时间/小节查询。
- 实现：`let H = harmony span 10 whole { "Dm9" @ 0 ... }`；`chordAt`/`chordAtBar`。
- 预期：7 个事件；0 → Dm9，7/2 → Bbmaj7，9 → A7b9；重叠时 start 最大者优先、同 start 后者优先。
- 实际：与预期一致；`BenchmarkSharedHarmonyTimeline`、`HarmonyTimelineQueriesBoundariesAndGaps`、
  `HarmonyTimelineOverlapDeterminism`、`HarmonyAcrossVariableMeter` 通过。
- 不支持：自动推断功能和声；区间级和声分析。

### SB-18 排列、转位、开放排列与省略/重复

- 指令：同一和弦可按音域、声部数、转位、最大间距生成不同排列；省略/重复必须可解释。
- 实现：`voicing(chord, low, high, voices, inversion, maxSpacing)`；MRP 使用第一转位 +
  maxSpacing 19 的开放排列。
- 预期：Dm9 根位 4 声部 = D3/F3/A3/C4；第一转位 = F3/A3/C4/E4；3 声部省略九音；5 声部重复低音。
- 实际：与预期一致；`VoicingBuildsInversionAndRange`、`VoicingOmissionAndDoublingRules`、
  `VoicingReportsImpossibleRange` 通过。
- 不支持：完整指法求解器；自动声部进行风格规则。

### SB-19 声部连接搜索与状态

- 指令：以总移动距离为目标，生成满足硬约束的下一排列；预算耗尽不得说成无解。
- 实现：`voiceLead(previous, chord, low, high, maxMove, maxSpacing, allowCrossing)`；
  MRP Reprise 使用 V0→V1→V2→V3 链。
- 预期：严格满足/容差内满足/不满足/预算耗尽/可证明不可满足可区分；候选确定。
- 实际：与预期一致；`BenchmarkVoiceLeadingInMrpReprise`、`BenchmarkVoiceLeadingStatusTaxonomy`、
  `VoiceLeadMinimizesMovement`、`VoiceLeadToleranceAndUnsatisfiedStatuses`、
  `VoiceLeadBudgetIsExplicit`、`VoiceLeadCrossingPolicy` 通过。
- 不支持：通用约束求解器；全局最优风格搜索。

### SB-20 独立节奏/和声替换

- 指令：节奏不变换和声、和声不变换节奏，并验证影响范围。
- 实现：`GuitarHarmonySwap`（同 R7、不同 HAlt）与 `GuitarRhythmSwap`（同 H、不同 R5）。
- 预期：同节奏替换和声 → 起音数相同、音高不同；同和声替换节奏 → 起音数不同。
- 实际：24 vs 24、16 vs 24，音高不同。
- 自动验证：`BenchmarkHarmonyAndRhythmReplacement`。
- 不支持：隐式风格生成器。

### SB-21 最小模块与标准库

- 指令：复用节奏/和声/排列算法，模块公开符号可导入，私有符号不冲突，依赖可追溯。
- 实现：`import "std/rhythm.mpl"` 等；MRP 使用 `arpBarAt`、`voicedBarAt`、`bassBarAt`、
  `leadBarAt`、`closeVoicing` 等。
- 预期：4 个模块记录；内容哈希非零；Trace 含 modules/imports/exports；21 个函数定义。
- 实际：与预期一致；`BenchmarkModuleIdentityAndStdlibUse`、`ModuleImportsExportsAndPrivateHelpers`、
  `ModulePrivateSymbolsDoNotCollide`、`ModuleTransitiveImportsResolve` 通过。
- 不支持：包管理器、版本约束、别名、重导出、动态模块。

### SB-22 音乐关系分析

- 指令：可查询音程、音域、和弦内音、共同音、声部移动、交叉、持续区间重叠；显式验证平行运动。
- 实现：`interval`、`pitchRange`、`fragmentRange`、`chordContains`、`commonTones`、
  `voiceDistance`、`voiceCrossings`、`noteOverlap`、`parallelPerfects`。
- 预期：GuitarA 与 BassA 存在持续区间重叠；平行五度显式计数为 1。
- 实际：与预期一致；`BenchmarkAnalysisAndRelationValidation`、`AnalysisOverlapRangeAndParallelMotion`、
  `AnalysisBuiltinsAreAvailableInLanguage` 通过。
- 不支持：默认风格判错；区间级约束求解。

### SB-23 乐器能力边界

- 指令：定义吉他音域、复音、弦/品位置，但不污染通用音高/和声语义。
- 实现：`InstrumentCapability`、`guitarCapability()`。
- 预期：E2..E6，6 复音，24 品；E3 可在低 E 弦 12 品或 D 弦 2 品；E7 不可演奏。
- 实际：与预期一致；`InstrumentCapabilityGuitarBoundaries` 通过。
- 不支持：完整指法求解器/物理建模。

### SB-24 Beta 语义修正

- 指令：复核 bars 锚定、Rhythm 切分、MIDI grouping 与不支持诊断。
- 预期：
  - 正分数 bars 在非小节锚点报 `meter-metric-duration`；
  - `rotate` 跨周期切分不自动 retrigger，`retrigger` 显式重新触发；
  - MIDI 只导出时间签名，grouping 在 Production/Trace/describe；
  - MRP 921,818 帧。
- 实际：全部通过；`MeterMapResolvesFractionalBarsFromAnchor`、`SplitContinuationDoesNotRetrigger`、
  `RhythmRotateWrapsAndSplitsCrossBoundaryEvents`。
- 不支持：Tie。

## 3.1 判定状态

| 状态 | 含义 | 是否声称无解 |
| --- | --- | --- |
| `strictly-satisfied` | 全部硬约束与目标精确满足 | 否 |
| `satisfied-within-tolerance` | 在显式容差（语音连接为 +1 半音）内满足 | 否 |
| `unsatisfied` | 有候选但未满足硬约束/容差 | 否 |
| `backend-capability` | 请求超出后端/乐器能力 | 否 |
| `budget-exhausted` | 搜索预算耗尽 | **否** |
| `provably-unsatisfiable` | 完整枚举证明无候选 | 是 |

这六类状态在 `voicingStatus` / `voiceLead` 中显式返回；基准不把艺术偏好混入状态。

## 4. 艺术评价边界

以下问题**不**进入自动验收，也没有在 MPL 中实现：

- 7/8 的 2+2+3 是否比 3+2+2 更“数学摇滚”；
- 主题再现是否有情感效果；
- 节奏复杂度是否等于音乐价值；
- 音色、混音、母带是否符合某种风格；
- 情绪映射、Taste 学习、LLM 生成。

MPL 只保证：给定明确指令，时间、周期、分组、力度映射、MIDI/WAV 与 Trace 精确且可复现。审美判断由人或外部系统承担。
