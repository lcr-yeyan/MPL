# MPL V0.3.0-rc 开发报告：音高、和声与声部关系

日期：2026-11-25

## 1. 结论

V0.3.0-rc 在保留 V0.3.0-beta 全部 237 项测试的基础上，完成：

- Beta 语义复核与修正（bars 锚定一致性、Rhythm 跨周期 continuation/retrigger、MIDI grouping 边界）；
- PitchClass / PitchClassSet / ChordSymbol / Voicing / HarmonyTimeline；
- 共享和声时间线与查询；
- 节奏 × 和声 × 音高的显式组合；
- 有界声部连接搜索与六类状态；
- Analyze / Validate / Construct 关系接口；
- 乐器能力接口（吉他示例）；
- 最小模块、导入、公开符号、内容身份与标准库；
- MRP-001 升级（共享和声、扩展和弦、开放排列/转位、声部连接、独立替换、关系验证）；
- RC Semantic Benchmark。

全量测试 **277/277** 通过（Debug 与 Release）；新增 40 项，既有 237 项全部保留。

## 2. Beta 语义复核与修正

见 `docs/reports/v0.3.0-rc-architecture-review.md`。

### 2.1 Bars 锚定一致性

Beta 中正分数 bars 可在非小节 anchor 解析，而正整数 bars 要求小节起点。RC 统一：

- 所有正 bars 都要求小节边界；
- 整数逐小节行走，可跨拍号；
- 分数按 anchor 处小节长度缩放；
- 负 bars 线性位移；
- 失败 `meter-metric-duration`。

### 2.2 Rhythm 边界

- 新增 `RhythmEvent::continuation`；
- `rotate` 跨周期切分首段为 attack，后续为 continuation；
- attackCount/onset 集合/accent/关系算子只使用 attack；
- `rhythmCombine` 默认不 retrigger；
- `retrigger(rhythm)` 显式重新触发；
- Tie 未实现，文档明确。

### 2.3 MIDI grouping

- 时间签名事件完整导出；
- grouping 在 Production/Trace/describe；
- 不伪造 MIDI 元事件，不声称 MIDI 已表达分组。

### 2.4 复核结果

MRP-001 逻辑长度 10 whole notes、132 BPM、48 kHz，WAV 921,818 帧，与 Beta 报告一致。

## 3. 音高与和声类型

新增：

- `PitchClass`：0..11，无八度/拼写身份；
- `PitchClassSet`：去重排序集合；
- `ChordSymbol`：根音、质量、显式 interval set、slash bass；
- `Voicing`：具体排列 + 状态/移动/交叉/完整；
- `HarmonyTimeline`：有序和声事件。

和弦支持三和弦、6/7/9/11/13、变化音、转位；转位/扩展/重复/省略规则见
`docs/pitch-harmony-spec.md`。

## 4. 和声时间线

```text
let H = harmony span 10 whole {
  "Dm9" @ 0 whole for 7/4 whole,
  "G7b9" @ 7/4 whole for 7/4 whole
};
```

- 事件 `[start, start+duration)`；
- 允许重叠/空白；`eventAt` 取 start 最大者，同 start 后者优先；
- 边界右连续；gap 返回空和弦；
- `chordAt` / `chordAtBar`；
- Music IR `harmony-literal` / `harmony-query`；
- 共享 H 与替换 HAlt。

MRP 实际：

```
0         Dm9
7/4       G7b9
7/2       Bbmaj7
19/4      A7b9
6         Dm9
8         Bbmaj7
9         A7b9
span      10
```

## 5. 声部排列与连接

- `buildVoicing`：音域、声部数、转位、最大间距、重复/省略规则；
- `voiceLead`：硬约束 + 总移动距离最小 + 确定性候选排序 + 有界预算；
- 状态：`strictly-satisfied` / `satisfied-within-tolerance` / `unsatisfied` /
  `backend-capability` / `budget-exhausted` / `provably-unsatisfiable`；
- MRP Reprise 使用第一转位、maxSpacing 19 的开放排列与 V0→V1→V2→V3 连接。

设计决定见 ADR-0028。

## 6. 关系分析

Analyze：interval/pitchClass/pitchRange/fragmentRange/chordContains/commonTones/
voiceDistance/voiceCrossings/noteOverlap；
Validate：voicingStatus/voicingComplete/parallelPerfects；
Construct：voicing/voiceLead/zip/cycle/stdlib 组合。

默认不把非和弦音、平行运动、交叉判错；只有显式规则才成立。设计决定见 ADR-0029。

## 7. 最小模块系统

- `module Name;` / `import "path";` / `export fn/let/curve`；
- 链接期合并 AST；私有符号 mangle；
- 循环/缺失/导出冲突/不支持声明诊断；
- 模块内容哈希与 imports/exports 进入 Trace；
- 标准库：StdRhythm / StdHarmony / StdVoicing。

设计决定见 ADR-0030，规范见 `docs/module-spec.md`。

## 8. 乐器能力

- `InstrumentCapability`：音域、复音、开放弦、品数、弦/品位置；
- 标准吉他 E2..E6、24 品、6 复音；
- 不进入 Pitch/Chord 通用语义；
- 不实现完整指法求解器。

设计决定见 ADR-0031。

## 9. MRP-001 升级

源：`examples/math-rock.mpl`

保留：

- 7/8 (2+2+3)、5/8 (2+3)、4/4 (2+2)；
- R3/R7/R5/R4、共同周期 21/8、realign/union、accentFollow；
- 主题 Theme/VerseTheme 与再现；
- 用户函数、Variant/Seed、MIDI/WAV。

新增：

- 共享和声时间线 H 与替换 HAlt；
- Dm9/G7b9/Bbmaj7/A7b9 扩展和弦与 A7#9；
- 第一转位、开放排列（maxSpacing 19）；
- Guitar/Bass 由和声驱动；
- Reprise 显式声部连接；
- 独立节奏/和声替换（GuitarRhythmSwap/GuitarHarmonySwap）；
- 关系验证（noteOverlap、voicingStatus、voiceMovement）；
- 导入标准库函数。

### 产物

```text
mplc build examples/math-rock.mpl -o build/math-rock --tail 1.0
```

| 属性 | 值 |
| --- | --- |
| Music IR | 144 objects |
| Modules | 4 |
| Production | 4 tracks, 14 clips, 247 notes |
| 逻辑长度 | 10 whole notes |
| 时长 | 18.18 s（不含尾音） |
| WAV | 48 kHz stereo float32, 921,818 frames |
| Sample Peak | -3.64 dBFS |
| True Peak | -3.26 dBTP |
| Integrated | -15.69 LUFS |
| Short-term max | -14.61 LUFS |
| Function definitions | 21 |
| Function calls | 63 |

## 10. 语义基准

见 `docs/reports/mrp-001-semantic-benchmark.md`。

23 项 Benchmark 测试覆盖 Beta 全部需求与 RC 新增：和弦构成、扩展/变化音、和声查询、
排列/转位/开放排列、声部连接状态、硬约束、确定性、局部修改、模块身份、乐器能力、
关系验证。

## 11. 测试

| 文件 | 数量 |
| --- | --- |
| test_core | 25 |
| test_audio | 13 |
| test_signal | 20 |
| test_analysis | 15 |
| test_temporal | 16 |
| test_functions | 15 |
| test_frontend | 15 |
| test_meter | 22 |
| test_rhythm | 27 |
| test_harmony | 22 |
| test_modules | 9 |
| test_relations | 13 |
| test_benchmark | 23 |
| test_backend | 9 |
| test_lowering | 8 |
| test_ir | 6 |
| test_golden | 6 |
| test_semantics | 13 |
| **合计** | **277** |

Debug 与 Release 均 277/277。

## 12. 文档与 ADR

- RC 架构审查：`docs/reports/v0.3.0-rc-architecture-review.md`
- 音高与和声规范：`docs/pitch-harmony-spec.md`
- 声部关系与优化契约：`docs/voice-relations-spec.md`
- 模块规范：`docs/module-spec.md`
- 更新 EBNF：`docs/grammar.ebnf`
- 语言规范 §16、算子规范 §11、类型系统
- ADR-0026 Beta 语义修正
- ADR-0027 和弦/排列/和声时间线
- ADR-0028 有界声部连接与状态
- ADR-0029 分析与验证分离
- ADR-0030 最小模块链接器
- ADR-0031 乐器能力边界
- ADR-0032 节奏/和声/音高独立替换
- Semantic Benchmark：`docs/reports/mrp-001-semantic-benchmark.md`

## 13. 范围限制

未实现：LLM、情绪映射、Taste、通用约束求解、完整泛型、任意递归、完整插件宿主、
SoundBoxer Adapter、大规模 DSP、完整微分音/调律、Tie、完整模块命名空间/包管理器、
完整吉他指法求解器、跨编译持久缓存。

## 14. 下一步

1. Tie / 跨周期连音线；
2. 完整模块命名空间/别名/版本约束；
3. 区间级声部关系与和声分析；
4. 完整 Tempo Map 与变速；
5. 跨编译增量缓存；
6. 受控递归与标准库扩展。

成功标准已满足：MPL 能精确、可控、可复现地表达时间、节奏、音高、和声与多声部组合，
不支持的语义有明确诊断而非隐藏近似。
