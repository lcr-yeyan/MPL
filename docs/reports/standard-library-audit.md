# Standard Library Audit（V0.5.0-alpha，完整）

日期：2026-11-25

审计对象：33 个知识单元、Core 算子、3 个 Stdlib 模块、Observation/Frame/Trusted
接口。目标是把“音乐计算能力”与“知识条目/Prompt 内容”分开。

## 1. 六类音乐计算能力

| 类别 | 定义 | 本仓库代表 |
| --- | --- | --- |
| 已实现计算原语 | 可直接调用的 Core 符号 | `note`、`rest`、`ratio`、`random`、`rotate`、`sequence`、`overlay`、`repeat`、`shift`、`stretch`、`subdivide`、`accent`、`realign`、`quantize`、`period`、`commonPeriod`、`onsetSet`、`zip`、`cycle`、`chord`、`chordAt`、`voicing`、`voiceLead` |
| 可组合音乐变换 | 由原语/Stdlib 组合的片段、节奏、和声、排列 | Stdlib 21 个函数、`harmony.arp`、`arrangement.*` |
| 音乐关系与分析 | 观察/分析而非生成 | `interval`、`pitchClass`、`pitchRange`、`fragmentRange`、`chordContains`、`commonTones`、`voiceDistance`、`noteOverlap`、`parallelPerfects`、`voicingCrossings` |
| 可执行约束 | 硬约束/状态/验证 | `voicing` 的 range/spacing、`voicingStatus`、`voiceLead` 硬约束、Frame Contract、Task Verifier、mastering target |
| 高级音乐算法 | 搜索/优化/多阶段 | `voiceLead` 有界最小移动；mastering 反馈控制；`motivic development` 为 deferred |
| 非强制实践建议 | advisory，不是语言合法性 | `practice.voice-leading-preference` |

## 2. 能力矩阵

| 领域 | 可直接调用符号 | 语法级计算 | Stdlib 组合 | 观察/验证 | 运行时 | 未实现候选 |
| --- | --- | --- | --- | --- | --- | --- |
| Rhythm | `sequence`/`overlay`/`repeat`/`rotate`/`shift`/`stretch`/`subdivide`/`accent`/`realign`/`quantize`/`period`/`commonPeriod`/`onsetSet`/`zip`/`cycle`/`intersect`/`union`/`difference`/`complement`/`accentFollow` | `rhythm { hit/rest }` | `pulseGrid`、`accentGrid`、`accentRhythm` | `onset_set`、`normalized_rhythm` | 精确 Rational/MeterMap | tie、列表索引、跨小节持续音 |
| Melody | `sequence`、`transpose`、`diatonicTranspose`、`syncopate` | `note`/`rest`/`let` | 无独立旋律模块 | `pitch_sequence`、`interval_sequence`、`fragment_range` | Fragment 求值 | 动机发展、主题变形库 |
| Harmony | `chord`、`harmony`、`chordAt`、`chordAtBar`、`voicing`、`voicingNotes` | `harmony { "Dm9" @ ... }` | `chordNotes`、`voicedBarAt`、`arpBarAt`、`bassBarAt`、`leadBarAt`、`arpBar`、`leadBar`、`harmonyBarChord` | `harmonic_timeline` | 和弦/时间线求值 | 功能分析、转调模型 |
| Voicing | `voicing`、`voicingNotes`、`voicingStatus`、`voicingMovement`、`voicingCrossings`、`voicingComplete`、`voiceLead`、`commonTones`、`voiceDistance`、`parallelPerfects` | 无 | `closeVoicing`、`nextVoicing`、`nextVoicingResult`、`sharedTones` | `fragment_range`、`voice_movement` | Voicing 值 | 独立 range/spacing 查询、完整指法 |
| Arrangement | `sequence` | `section`/`track`/`clip`/`arrangement` | 无 arrangement 模块 | `logical_extent`、`track_symbolic_events` | 多 Section 拼接/渲染 | Section 一等值、结构复用、局部编辑语言原语 |
| Production | 无独立函数；track gain/pan、processor、automation 为语法 | `track`、`bus`、`master`、`processor`、`automation` | 无 production 模块 | `sample_peak`、`true_peak`、`integrated_loudness` | 48k WAV、处理器图、mastering | 完整 tempo map、动态图构造 |
| 跨域 | `Semantic Delta`、`Frame Contract`、Task Verifier | `export` | 无 | `task`/`frame`/`trusted manifest` | 确定性渲染 | 通用约束求解（明确不做） |

## 3. 语言符号 vs 语法 vs Stdlib vs 验证

| 类型 | 数量/内容 |
| --- | --- |
| 可直接调用 Core 符号 | 数十个；上表列出的节奏/和声/声部/关系算子 |
| 语法级计算 | `rhythm` 字面量、`harmony` 字面量、`section/track/clip`、track gain/pan、automation、processor |
| Stdlib 导出函数 | **21**：StdHarmony 8、StdRhythm 3、StdVoicing 7、StdAnalysis 3 |
| 观察/验证接口 | Observation Registry、Task Verifier、Frame、Semantic Delta、Trusted Render |
| 运行时能力 | 精确时间、MIDI、48k WAV、处理器图、mastering、离线执行 |
| 未实现候选 | 列表 append/filter/fold、完整 Tempo Map、Section 一等值、arrangement/production 标准库 |

## 4. 33 个知识单元的分类

| 类别 | 数量 | 单元 |
| --- | --- | --- |
| 已实现计算原语 | 16 | rhythm.hit-rest、rhythm.subdivide、rhythm.rotate、rhythm.stretch、rhythm.common-period、melody.transpose、melody.diatonic-transpose、harmony.chord、harmony.timeline、harmony.query、harmony.voicing、voicing.inversion、production.gain、production.pan、production.automation、production.processor-chain |
| 可组合变换 | 8 | rhythm.onset-relations、melody.motif、melody.rhythmic-variation、harmony.arp、arrangement.section、arrangement.theme-reuse、arrangement.rhythm-replacement、arrangement.harmony-replacement |
| 音乐关系/分析 | 2 | melody.range、voicing.crossing |
| 可执行约束 | 4 | voicing.range、voicing.spacing、arrangement.local-edit、production.mastering-target |
| 高级音乐算法 | 2 | voicing.voice-leading、arrangement.motivic-development（deferred） |
| 非强制实践建议 | 1 | practice.voice-leading-preference |
| **合计** | **33** | — |

注意：这是**知识语义分类**，不是 33 个函数。知识条目可以引用同一实现（例如
`core:voicing` 被 4 个单元引用），也可以引用语法而不是函数。

## 5. 实现引用审计

| 引用前缀 | 单元数 | 唯一引用 | 说明 |
| --- | --- | --- | --- |
| `core:` | 19 | 14 | 14 个唯一 Core 引用，存在重复包装 |
| `syntax:` | 7 | 7 | 语法节点/轨道参数，不是函数 |
| `stdlib:` | 3 | 2 | `arpBarAt` 被两处引用，`voicedBarAt` 一次 |
| `observation:` | 1 | 1 | `fragment_range` |
| `task:` | 1 | 1 | Frame/Task 验证 |
| `benchmark:` | 1 | 1 | mastering target |
| `deferred:` | 1 | 1 | 无实现 |
| **合计** | **33** | **27** | — |

重复引用：

- `core:voicing` ×4（harmony.voicing、voicing.inversion、voicing.range、voicing.spacing）；
- `core:sequence` ×2（melody.motif、arrangement.theme-reuse）；
- `core:voiceLead` ×2（voicing.voice-leading、practice）；
- `stdlib:arpBarAt` ×2（harmony.arp、arrangement.rhythm-replacement）。

## 6. 缺口、重复与语义不一致

### 缺口

1. **列表 append/filter/fold 缺失**：索引/切片已实现；累积式算法仍不可表达。
2. **Tie 仅边界级**：跨周期持续音已有显式 tie；voice/continuation 演奏层未实现。
3. **完整 Tempo Map 缺失**：只有初始 tempo 事件。
4. **Section 不是一等值**：无法把 Section 作为参数复用。
5. **没有 arrangement/production 标准库模块**；analysis 已有 3 个派生函数。
6. **无独立 `voicingRange`/`voicingSpacing` 查询**：只能靠 `voicing` 构造参数或状态。

### 重复/包装

- 4 个单元共享 `core:voicing`；2 个共享 `core:sequence`；2 个共享 `core:voiceLead`；
- `harmony.arp` 与 `arrangement.rhythm-replacement` 都引用 `arpBarAt`，语义不同但实现相同；
- 这些包装有上下文价值，但不能计为 4/2/2 个独立函数。

### 语义不一致

- `kind=executable` 的单元中有 7 个实际是 syntax-level（rhythm/harmony 字面量、
  section、track gain/pan、automation、processor），不是可调用 executable；
- `production.gain/pan` 是轨道参数语法，不是 `gain()`/`pan()`；
- `arrangement.local-edit` 的实现是 Task/Frame 验证器，不是音乐变换；
- `production.mastering-target` 是基准/验证，不是语言函数；
- 知识检查多为 `event_count`/`pitch_sequence` 示例级观察，不是符号级实现验证。

## 7. 建议

1. 将 `kind` 与 `implementationRef` 对齐：新增 `syntax`/`verification`/`benchmark` 种类，
   不把语法包装标为 executable。
2. 建立符号注册表：`core-op:`、`syntax:`、`stdlib:<module>.<fn>`、`observation:`、
   `task:`、`benchmark:`；解析失败标 Unverified。
3. 区分 `implementationRef`（实现）与 `contractRef`（验证证据）。
4. 增加通用 Stdlib 模块：Arrangement、Analysis；不添加模型专用函数。
5. P0/P1 语言扩展见 `language-expressiveness-audit.md`。
6. 不把 33 个知识条目宣传为 33 个音乐计算函数；真实 Stdlib 是 21 个函数。

## 8. 结论

- 已实现：Core 节奏/和声/声部算子、21 个 Stdlib 函数、Observation/Frame/Trusted；
- 可组合：21 个 Stdlib 函数 + 知识构造；
- 高级算法：有界 voice-leading；mastering 反馈；动机发展 deferred；
- 缺口：append/filter/fold、Tempo Map、Section 值、range/spacing 查询、arrangement 库；
- 33 个知识单元是语义/契约层，不是函数数量。

## 9. 依赖记录

stdlib/dependencies.json 记录 21 个导出函数到 Core 原语/语法能力的依赖；stdlib/manifest.json 记录模块、导出数、文件哈希与 contracts 哈希。
