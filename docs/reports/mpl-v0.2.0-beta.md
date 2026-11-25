# MPL V0.2.0-beta 开发报告：独立运行时与信号计算

日期：2026-11-25

> 后续：V0.2.0 收束版本新增音频分析、母带处理与生产反馈，测试总数 146。最终状态见 `docs/reports/mpl-v0.2.0-final.md`。

## 1. 结论

本阶段完成了从 MPL Source 到 WAV 的独立执行链：

```text
MPL Source
  → Music IR / Temporal IR
  → Production IR（含 MixSpec 与 logicalExtent）
  → Audio Execution Plan（含 SignalGraphPlan）
  → Signal Processing Graph
  → float32 WAV
```

`examples/production.mpl` 可在不启动、不链接、不依赖 SoundBoxer 的情况下编译并渲染出完整作品。全量测试 **131/131** 通过（Debug 与 Release 各一次），其中 V0.1.0 的 98 项测试全部保留。

## 2. 战略调整与边界

- 取消 V0.2.0-beta 必须接入 SoundBoxer 的要求。
- MPL 保持独立的产品、源码、构建、测试与运行时。
- 未引入 SoundBoxer 的 ProjectModel、AudioEngine、AudioGraph 或其它专有结构。
- 既有 `docs/reports/phase2-architecture-review.md` 保留，作为未来 Adapter 的输入；本阶段不实现 Adapter。
- 本阶段不实现：GUI、实时音频设备、完整插件宿主、AI/LLM、情绪映射、Taste 学习、完整空间音频、通用 DSP 语言。

关于是否另建 Native Audio Runtime：现有 `ReferenceAudioBackend` 已经是独立、无界面、可离线执行的 MPL 原生运行时，具备资产解析、事件展开、信号图执行、DSP 与 WAV 输出。再建第二套实现没有语义依据，因此本阶段直接将其作为 Native Audio Runtime 强化，而不复制其逻辑。MIDI Backend 保持完全独立，只共享 Production/Temporal IR。

## 3. 回归审查

### 3.1 测试口径更正

| 版本 | 测试数 | 说明 |
| --- | --- | --- |
| V0.0.3 | 83 | Core/IR/后端/时变/黄金基线 |
| V0.1.0 | 98 | 83 + 15 前端测试 |
| V0.2.0-alpha | 111 | 98 + 13 音频执行测试 |
| V0.2.0-beta | 131 | 111 + 20 信号/混音测试 |

此前报告中的“83 项既有测试”是**口径错误**：83 是 V0.0.3 Core 基线，不是 V0.1.0 的全部测试。V0.1.0 的 98 项测试在 Beta 中完整存在，未删除、未重命名、未跳过。

### 3.2 事件、控制与音频边界

- Event Stream：`PlannedNote`，采样精确。
- Control Stream：Track/Master 的 volume/pan，以及处理器参数自动化。
- Audio Stream：内部 stereo 分块缓冲。
- 资产解析：`AssetResolver` 在每个轨道 `main` 绑定上解析一次并固化；符号音色与 WAV 文件均支持。
- 参数注册：前端 track 参数仍由 `ParameterRegistry` 管理；处理器参数改由 `ProcessorParameterDescriptor` 管理，单位/范围/默认值与音频层共用同一张表。

### 3.3 曲线与 Block Size

- 线性/非线性曲线求值语义未改变；Step 取段右连续，Keyframe 严格递增，Repeat 按周期平铺。
- Track/Master 控制与处理器参数在块边界求值、块内线性插值。
- Biquad 截止频率逐样本 5 ms 平滑；Delay/Reverb/Compressor 逐样本读取插值参数。这样 Block Size 差异只来自曲线弦逼近，不再来自“整块跳到终点值”。
- 测试 `ProcessorAutomationIsBlockSizeStable` 在 64/512 下比较整首作品，最大样本差 < 2e-3。
- `StepAutomationLandsWithinOneBlock` 验证 Step 不连续点在一个 Block 内落地。

### 3.4 渲染长度与留白

- V0.2.0-alpha 的“三类时长”升级为四类：Logical / Content / Processor Tail / Render。
- `logicalExtent` 由 arrangement 总时长写入 Production IR；`LogicalPaddingSurvivesRender` 验证 2 小节段落中仅 1/4 音符时仍渲染满 4 秒且尾部为静音。
- `SilenceThreshold` 裁剪不得早于 logical extent；`MaxTailIsCappedWithDiagnostic` 验证 `maxTailSeconds` 截断与 `audio-tail-capped`。

## 4. 架构与实现

### 4.1 新增/修改文件

| 文件 | 内容 |
| --- | --- |
| `include/mpl/ir/MixIR.hpp`, `src/ir/MixIR.cpp` | ProcessorSpec、ChannelStripSpec、BusSpec、Send/Sidechain、ProcessorAutomationSpec；参数描述符 |
| `include/mpl/frontend/Ast.hpp` | Processor/Bus/Master 声明、Track 混音字段、三段 automation |
| `src/frontend/Lexer.cpp`, `Token.hpp` | `processor`、`bus`、`insert`、`send`、`sidechain` 关键字 |
| `src/frontend/Parser.cpp` | 新声明、Track 字段、三段 automation 解析 |
| `src/frontend/Sema.cpp` | 处理器/总线符号、参数单位与范围、insert/send/sidechain、自动化校验 |
| `src/frontend/Lowering.cpp` | MixSpec 构建、processorAutomation、logicalExtent |
| `src/frontend/Ast.cpp` | AST JSON 新字段 |
| `include/mpl/audio/AudioExecution.hpp`, `AudioExecution.cpp` | SignalGraphPlan、DSP 计划、能力检查、图验证、extent 计算 |
| `include/mpl/audio/Processors.hpp`, `Processors.cpp` | Gain/Pan/Biquad/Compressor/Delay/Reverb 与参数 ramp |
| `src/audio/ReferenceAudioBackend.cpp` | 按图拓扑执行、sidechain、状态复位、静音终止 |
| `include/mpl/audio/BackendContract.hpp` | BackendCapabilities、UnsupportedPolicy、RenderTermination |
| `examples/production.mpl` | Production Demo |
| `tests/test_signal.cpp` | 20 项新测试 |
| `docs/audio-execution-spec.md`、`grammar.ebnf`、`language-spec.md` 等 | 规范更新 |
| `docs/design-decisions/ADR-0014/0015` | 信号图与独立运行时决策 |

### 4.2 Backend Contract

- `BackendCapabilities` 覆盖事件精度、处理器自动化、sidechain、Bus/Send、静音终止、最大声道、处理器集合。
- `UnsupportedPolicy`：
  - Strict：错误并拒绝；
  - Approximate：`audio-approximated-processor` 警告；
  - Ignore：`audio-ignored-processor` 警告。
- 声道超限：`audio-unsupported-channels`。
- MIDI Backend 与 Reference Backend 独立；两者只共享 Production/Temporal IR。

### 4.3 信号图

```text
TrackSource → Processor… → Track Gain → Track Pan → MasterMixer
                               └── send gain → BusMixer → Processor… → Bus Gain/Pan → MasterMixer
MasterMixer → Master Processor… → Master Gain → Master Pan → Output
Sidechain: TrackSource → Compressor.key
```

- 节点：Source / Mixer / Processor / Output。
- 连接：`{from, to, gain, sidechain}`。
- 拓扑排序确定性；本阶段拒绝全部环路。
- Source 无音频输入；Processor 单主输入 + 单 sidechain 输入；Output 恰好一个输入。
- stereo→mono 拒绝；mono 源在 Source 升为 stereo。
- 每次渲染 `prepare/reset`，块间保持状态。

### 4.4 基础 DSP

| 处理器 | 参数与单位 | 实现要点 |
| --- | --- | --- |
| Gain | gain dB | 线性增益；自动化线性乘子 |
| Pan | pan -1..1 | stereo balance |
| Mixer | 连接增益 | 求和 |
| Biquad | mode、frequency Hz、q、gain dB | RBJ 五种模式，5 ms 截止平滑 |
| Compressor | threshold dB、ratio、attack/release s、makeup dB | 峰值检测、软起音/释放、sidechain |
| Delay | time s、feedback、mix | 线性插值延迟线，有限尾音 |
| Reverb | room、damping、mix | comb/allpass，有限尾音 |

### 4.5 混音语言

```text
processor Eq = biquad { mode = peaking; frequency = 1200 Hz; q = 1.2; gain = 4 dB; };
bus Reverb { insert Hall; gain = -4 dB; }
track Piano {
  asset = GrandPiano; role = melodic;
  gain = -2 dB; pan = -0.15;
  insert Eq; send 0.3 to Reverb;
}
master { insert MasterComp; gain = -1 dB; }
automation Piano.Eq.frequency = PianoSweep;
automation Reverb.Hall.mix = HallDepth;
automation master.MasterComp.threshold = DuckCurve;
```

- `insert` 顺序 = 处理器链顺序。
- Send 为后推子发送，level 0..1。
- Bus 求和 → 插入链 → Bus Gain/Pan → Master。
- Master 链之后进入 Output。
- Sidechain 键信号取源轨 Source（pre-insert、pre-fader）。
- 三段自动化目标为 `<strip>.<processor>.<parameter>`，strip 可为 Track、Bus、`master`。
- 符号解析要求 Processor/Bus 先声明；Master 块在全部声明后校验。

## 5. 验证

### 5.1 新增测试（20 项）

| 测试 | 覆盖 |
| --- | --- |
| `SignalGraphBuildsMixTopology` | 插入链、Send、Sidechain 边、Master 链、拓扑序 |
| `SignalGraphRejectsIllegalTopology` | cycle、声道不兼容、Source 输入、重复 sidechain、缺 Output |
| `SignalGraphCapabilityPolicies` | Strict / Approximate / Ignore、声道能力 |
| `DspGainAndPan` | 增益数值、左右声像 |
| `DspEqFrequencyResponse` | lowpass 衰减、peaking +6 dB |
| `DspCompressorDynamicAndSidechain` | 压缩比、阈值以下透明、sidechain 衰减 |
| `DspDelayImpulsePosition` | 80 sample 延迟位置与干湿比 |
| `DspReverbTailAndReset` | 尾音能量、衰减、reset 清零 |
| `MixLanguageLowersStripsAndAutomation` | MixSpec、Bus、Send、Sidechain、processorAutomation、logicalExtent |
| `MixLanguageDiagnostics` | 未知处理器/参数、单位、范围、mode、insert、bus、sidechain、自动化目标/range/domain |
| `BusSendAndReturnAffectsOutput` | Send/Return 对输出能量的影响 |
| `SidechainDucksTheTargetTrack` | 实际混音路径的 ducking |
| `RendererAppliesSidechainKey` | 手工信号图的 sidechain 键输入 |
| `LogicalPaddingSurvivesRender` | 逻辑留白不被删除 |
| `RenderTerminationPolicies` | Fixed 与 SilenceThreshold |
| `StepAutomationLandsWithinOneBlock` | Step 不连续点位置 |
| `MaxTailIsCappedWithDiagnostic` | maxTail 截断与诊断 |
| `ProcessorAutomationIsBlockSizeStable` | 64/512 Block 误差容差 |
| `RenderingIsDeterministicAndResetsState` | 两次渲染逐样本一致 |
| `ProductionDemoRendersGoldenStats` | Production Demo 结构与音频黄金 |

### 5.2 测试策略

- 结构验证：AST/Production IR/MixSpec/SignalGraph 的字段与连接。
- 数值容差：EQ 频率响应、Compressor 动态、Delay 位置、Send/Sidechain 能量比、Block Size 误差。
- 固定环境黄金：Production IR/Music IR/Temporal IR/Trace/MIDI hex，以及音频帧数、peak、RMS、FNV-1a hash。

### 5.3 黄金更新

- `core-demo.production.ir`、`temporal-demo.production.ir`、`reference.production.ir`：新增 `logicalExtentWholeNotes` 与 `mix`（空 processors/strips/master 默认值）。
- `audio-reference.json`：信号图执行与逐样本参数读取后的新统计值。
- `production-demo.json`：新增 Production Demo 音频统计。
- AST 黄金未变化：新字段仅在存在时输出。

## 6. Production Demo

`examples/production.mpl`：

- 3 段落：Intro（2 小节）、Verse（4）、Chorus（4）。
- 5 轨：Piano、Pad、Drums、Bass、Lead，多种音色。
- 演奏表情：velocity 渐强、timing humanize。
- 自动化：Piano/Pad volume、Piano EQ frequency、Bass compressor threshold、Reverb mix、Master compressor threshold。
- EQ、Compressor、Reverb Send、Sidechain、Master Bus。
- 导出 MIDI 与 WAV。

实际渲染：

```text
mplc build examples/production.mpl -o build/production --tail 1.5
```

| 属性 | 值 |
| --- | --- |
| 文件 | `build/production/production.wav` |
| 采样率 / 声道 | 48000 / stereo |
| 帧数 | 1,530,000（31.875 s） |
| 大小 | 12,240,044 bytes |
| peak | 0.410801 |
| RMS | 0.049688 |
| 依赖 | 仅 MPL 自身；无 SoundBoxer |

MIDI 导出对 humanize timing 产生 `midi-quantization` 警告，属于预期：音频路径保留亚 tick 偏移，MIDI 路径量化到 tick。

## 7. 已知限制

- 振荡器无抗混叠；采样播放为简单重采样。
- 反馈回路全部拒绝；尚未实现带显式延迟的合法反馈。
- 非线性自动化在 Block 边界采样，存在容差级误差。
- 无实时音频设备、插件宿主、GUI、audio-rate modulation、空间音频。
- Reverb/Delay 为教学级算法，不追求商业音质。

## 8. 后续建议

1. 设计 SoundBoxer Adapter（独立模块，不进入 MPL Core）。
2. 引入显式延迟反馈节点与图级延迟约束。
3. 增加物理/采样域曲线与 audio-rate modulation 接口。
4. 扩展混音语言：Bus Send、Processor 旁链滤波、Master 限幅器。
5. 增加更多黄金音频场景与性能基准。
