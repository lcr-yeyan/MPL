# MPL 音频执行规范（V0.2.0-beta）

## 1. 目标

定义从 Production IR 到实际音频的稳定语义边界，并由 MPL 自带的 Reference Audio Backend 独立执行。成功标准是：不依赖任何 DAW 产品，即可完成基础作曲、编曲、音频执行与混音。

## 2. 执行边界与流类型

| 概念 | 实现 |
| --- | --- |
| Audio Stream | 分块渲染的交错 float 缓冲，内部端口为 stereo |
| Event Stream | `PlannedNote`：采样精确的 Note On/Off、力度、声像 |
| Control Stream | `ControlBindingPlan` / `ProcessorAutomationPlan`：按音乐时间求值 |
| Audio Port | 节点 `inputChannels` / `outputChannels`；mono 源在 Source 升为 stereo |
| Control Port | 处理器参数端口，按 `ParameterRamp` 在块内插值 |
| Processor | `AudioProcessor`：`prepare` / `reset` / `process`，可选 sidechain 输入 |
| Bus | 多输入求和节点，带插入链与 Gain/Pan |
| Connection | `SignalConnectionPlan{from, to, gain, sidechain}` |
| Sidechain Input | Compressor 的第二个音频输入，来自源轨 Source 输出 |

`AudioExecutionPlan` 是后端无关的执行计划；Reference Audio Backend 是它的参考实现与测试 oracle。

## 3. 时间与采样

```text
MusicalTime (whole notes, Rational)
  → PhysicalTime (seconds, tempo map)
  → SampleTime (int64 frames)
```

- Tempo map 为分段常速；`AudioClock` 提供 `secondsAtWhole` / `wholeAtSeconds`。
- 音符起点/终点：`llround(seconds × sampleRate)`；起点不小于 0，终点至少为起点 + 1 sample。
- 事件位置与 Block Size 无关；Block 只影响处理循环与参数求值粒度。

### 3.1 四种 extent

| 名称 | 定义 | 是否截断音频 |
| --- | --- | --- |
| Logical Extent | arrangement 总时长，包含有意留白 | 否 |
| Content Extent | 所有音符结束的最大值 | 否 |
| Processor Tail | Delay/Reverb 等有限状态的衰减长度 | 否 |
| Render Extent | `max(logical, content + processorTail) + tail` | 是 |

```text
base = max(logicalExtent, contentExtent + processorTail)
renderExtent = min(base + tailSeconds, base + maxTailSeconds)
```

- Section 越界只产生警告；音频不会在 section 边界截断。
- 最后一个音符提前结束不得删除 logical extent 的留白。
- `infinite` 状态处理器（Reverb 等）由 `maxTailSeconds` 截断并产生 `audio-tail-capped` 警告。
- `SilenceThreshold` 可在 logical/content 边界之后裁剪静音，但不得早于 `logicalExtent`。

## 4. Audio Execution Plan 与信号图

```text
AudioExecutionPlan
├── config (sampleRate, blockSize, channels, tailSeconds, maxTailSeconds,
│           silenceThresholdDb, termination, policy)
├── tempo segments
├── TrackPlan[]
│   ├── InstrumentAsset (resolved, versioned)
│   ├── PlannedNote[]
│   └── ControlBindingPlan[] (volume/pan)
├── masterControls
├── SignalGraphPlan
│   ├── SignalNodePlan[] (Source / Mixer / Processor / Output)
│   ├── SignalConnectionPlan[]
│   ├── order (deterministic topological order)
│   └── outputNode
└── extents (logical / content / processorTail / rendered)
```

### 4.1 图拓扑

```text
TrackSource → Processor… → Track Gain → Track Pan → MasterMixer
                               └── send gain → BusMixer → Processor… → Bus Gain/Pan → MasterMixer
MasterMixer → Master Processor… → Master Gain → Master Pan → Output
Sidechain: TrackSource → Compressor.key
```

- 节点按拓扑序执行；同层按节点 id 升序，保证确定性。
- 本阶段拒绝所有环路（`graph-cycle`）。未来带明确最小延迟的合法反馈必须由 Delay 节点显式声明，本阶段不实现。
- 普通音频端口为 stereo；mono 源在 Source 升为 stereo；stereo→mono 连接拒绝（`graph-channel-mismatch`）。
- Source 不允许音频输入；Processor 只允许一个主输入与一个 sidechain 输入；Output 恰好一个输入。
- 每个处理器实例在渲染开始 `prepare/reset`；块间保持状态，重渲染从头复位。

### 4.2 资产与参数

- 资产在每个 track 的 `main` AssetBinding 上解析一次并固化到计划（id/version）。
- 同一音乐程序可替换 Resolver，而不改变音符/时间结构。
- 缺失资产/未知音色 → `audio-missing-asset` / `audio-unknown-asset` 错误，不静默静音。
- 处理器参数由 `ProcessorParameterDescriptor` 描述单位、范围与默认值；前端与音频层共用同一张表。

## 5. 自动化

- 两段目标 `<strip>.<parameter>`：Track/Master 的 `volume`/`pan`，Musical 域。
- 三段目标 `<strip>.<processor>.<parameter>`：处理器参数自动化；strip 可以是 Track、Bus 或 `master`。
- 组合模式沿用 Core：Replace 为基值，Add 求和，Multiply 求积。
- Track volume/pan 与处理器参数在块边界求值，块内线性插值；滤波器截止频率额外做 5 ms 一阶平滑。
- 音符事件仍是采样精确。
- 非线性曲线在 Block 边界采样，存在 Block 相关近似误差；测试以容差验证 64/512 的差异。
- 不支持的参数（如 modulation）或 clip 作用域 → `audio-unsupported-parameter` / `audio-unsupported-automation` 警告。

## 6. 基础 DSP

| 处理器 | 参数 | 语义 |
| --- | --- | --- |
| Gain | gain dB、linear（内部） | 线性增益；自动化以线性乘子叠加 |
| Pan | pan -1..1 | stereo balance |
| Mixer | 连接增益 | 多输入求和 |
| Biquad | mode、frequency、q、gain | RBJ lowpass/highpass/peaking/lowshelf/highshelf |
| Compressor | threshold、ratio、attack、release、makeup | 峰值检测、软起音/释放、可选 sidechain |
| Delay | time、feedback、mix | 立体声延迟线、线性插值读取、有限尾音 |
| Reverb | room、damping、mix | Freeverb 风格 comb/allpass，有限尾音 |
| Limiter | ceiling dB、attack/release s | 前视采样峰值限制；attack 即内部延迟；不保证 dBTP |

数值优先：参数单位、范围、默认值、状态复位、尾音可解释。第三方 DSP 库可作为独立依赖，但 MPL Core 不引入 SoundBoxer 产品级依赖。

### 6.1 处理器延迟与尾音

- **Processor Latency** 是为保证处理正确性引入的内部延迟，例如 Limiter 的前视时间。它必须被补偿，否则音乐位置会平移。
- **Processor Tail** 是处理器状态在输入结束后继续发声的长度，例如 Reverb/Delay 的衰减。它只影响渲染长度，不改变音乐位置。
- 艺术性 Delay 的 `time` 不是 Processor Latency，不做自动补偿。
- 执行计划按主信号路径计算累计延迟；若同一节点的输入路径延迟不一致，产生 `audio-latency-misaligned` 诊断。本阶段不实现通用延迟补偿（PDC）。
- 所有路径延迟一致时，渲染器在最终输出中裁掉前导延迟，使第 0 帧仍对应音乐时间 0；渲染长度相应增加 latencyFrames。
- Sidechain 键信号不参与延迟对齐；需要精确对齐时由未来 Adapter/图扩展处理。
- Limiter 的 `attack` 决定其延迟，且不可被自动化（`sema-processor-automation`）。

### 6.2 幅值、Pan Law 与导出

- 内部与输出均为 float32；不做隐式限幅、归一化或抖动。
- 若最终峰值超过 1.0，产生 `audio-peak-over` 警告，样本原样保留，由母带/宿主决定处理。
- 逐音符 pan 使用等功率 law（mono 源升为 stereo）；Track/Bus 的 Pan 是 stereo balance（中心不平衰减）。
- Send 为 post-fader：Track Gain/Pan 之后送入 Bus。
- Sidechain 键信号取源轨 Source（pre-insert、pre-fader）；Compressor 检测器取主输入与键输入的逐声道绝对值最大值。
- 所有反馈仍按信号图环路规则拒绝；Production Feedback 不属于 SignalGraph。

## 7. 后端契约与不支持语义

```text
BackendCapabilities
├── sampleAccurateEvents
├── processorAutomation
├── sidechain
├── busesAndSends
├── silenceTermination
├── maxChannels
└── processors[]
```

- `UnsupportedPolicy::Strict`：能力不足产生错误，拒绝渲染。
- `UnsupportedPolicy::Approximate`：产生 `audio-approximated-processor` 警告，允许近似执行。
- `UnsupportedPolicy::Ignore`：产生 `audio-ignored-processor` 警告并忽略该节点。
- 声道超限产生 `audio-unsupported-channels`。
- MIDI Backend 与 Reference Backend 保持独立；二者只共享 Production IR/Temporal IR。

## 8. 诊断

| 错误码 | 含义 |
| --- | --- |
| `audio-missing-binding` | 轨道没有资产绑定 |
| `audio-missing-asset` | 资产无法解析/文件缺失/WAV 读取失败 |
| `audio-unknown-asset` | 参考库没有对应的符号音色 |
| `audio-unsupported-parameter` | 参数无音频映射 |
| `audio-unsupported-automation` | 非 musical 域或 clip 作用域自动化 |
| `audio-unsupported-processor` | Strict 策略下后端不支持处理器 |
| `audio-approximated-processor` | Approximate 策略下的处理器近似 |
| `audio-ignored-processor` | Ignore 策略下忽略处理器 |
| `audio-unsupported-channels` | 节点声道数超过后端能力 |
| `audio-tail-capped` | 尾音被 `maxTailSeconds` 截断 |
| `graph-empty` / `graph-node-id` | 图结构非法 |
| `graph-connection` / `graph-port` | 连接引用/端口非法 |
| `graph-channel-mismatch` | stereo 输出连接 mono 输入 |
| `graph-source-input` | Source 节点出现音频输入 |
| `graph-fan-in` | Processor 多个主输入 |
| `graph-sidechain` | sidechain 目标/源非法或重复 |
| `graph-output` | Output 数量/输入非法 |
| `graph-cycle` | 存在未声明延迟的反馈回路 |
| `mix-missing-processor` / `mix-missing-bus` | 混音引用未定义对象 |
| `mix-sidechain-target` / `mix-sidechain-source` | sidechain 引用不存在 |
| `mix-duplicate-insert` | 同一通道条重复插入同一处理器 |
| `wav-read` / `wav-write` | WAV 解析/写入失败 |
| `audio-config` | 非法采样率/Block/声道配置 |
| `audio-latency-misaligned` | 并行路径处理延迟不一致 |
| `audio-peak-over` | 最终样本峰值超过 1.0，未归一化 |
| `analysis-config` | 分析配置非法 |
| `analysis-empty-range` | 分析区间为空 |
| `analysis-non-finite` | 输入含 NaN/Inf，已按 0 处理 |
| `mastering-silent` | 渲染静音，无法评估响度目标 |
| `mastering-unsatisfiable` | 目标在给定 ceiling/迭代预算下不可满足 |
| `mastering-peak-exceeded` | 最终 True Peak 超过目标 ceiling |
| `mastering-max-iterations` | 达到最大反馈迭代次数 |

`mplc build --json` 输出前端（带 span）与 Core/音频（无 span）诊断的统一 JSON 数组。

## 9. Production Feedback（母带目标）

Production Feedback 是跨渲染阶段的离线闭环，不属于 SignalGraphPlan，也不产生图环路：

```text
Compile → Render → Analyze → Adjust master gain / limiter ceiling → Render → Verify
```

- 目标（target）：Integrated Loudness，单位 LUFS。
- 约束（constraint）：True Peak ceiling，单位 dBTP。
- 容差相互独立：`loudnessToleranceLu`（默认 0.5 LU）与 `peakToleranceDb`（默认 0.1 dB）。
- 状态区分：`strictly-satisfied`、`satisfied-within-tolerance`、`unsatisfied`、`iteration-exhausted`、`numerical-failure`、`provably-unsatisfiable`。
- 最大迭代次数：默认 4，允许 0..16。
- 每轮只做确定性增益求解与 limiter ceiling 补偿，不做通用约束求解。
- 每轮记录：iteration、masterGainDb、limiterCeilingDb、完整 AudioAnalysis。
- 最终结果记录：before/after 分析、最终增益、limiter 参数、post-fader sample ceiling、状态与完整 Trace。
- Limiter 位于 Master Fader 之前：反馈按 `postFaderSampleCeiling = limiterCeiling + masterGain` 补偿，使最终 Sample Peak 落在目标附近；True Peak 仍由分析独立测量。
- 失败语义：静音/数值失败 → `mastering-silent`；不可满足 → `mastering-unsatisfiable`；True Peak 超限 → `mastering-peak-exceeded`；迭代预算耗尽 → `mastering-max-iterations`；仅超过严格 ceiling 但在 peak tolerance 内 → `mastering-peak-tolerance` 警告，状态为 `satisfied-within-tolerance`。
- Limiter 是采样峰值限制器；反馈会验证 dBTP，但不会把该 limiter 宣称为 True Peak Limiter。

CLI：

```text
mplc analyze mix.wav --json
mplc master examples/production.mpl -o build/mastering --target-lufs -14 --peak-ceiling -0.5
```

规范与参考值见 `docs/audio-analysis-spec.md`。

## 10. 独立运行时边界

- MPL 拥有独立的源码、构建、测试与运行时；`mplc build` 可直接生成 WAV。
- SoundBoxer 是未来可选的执行后端/交互环境之一，不是本阶段的依赖。
- 不引入 SoundBoxer 的 ProjectModel、AudioEngine、AudioGraph 或其他专有结构。
- 既有 SoundBoxer 架构审查保留，作为未来 Adapter 开发的参考。

## 11. 已知限制

- 参考后端不是商业级音源；振荡器无抗混叠。
- 采样播放为最近点/简单重采样；无循环、切片、速度同步。
- 非线性自动化曲线在 Block 边界采样，存在 Block 相关近似误差；线性自动化一致。
- 无实时调制、MPE、微分音、空间音频、插件宿主、GUI、实时音频设备。
- 所有反馈回路本阶段拒绝；尚未实现带显式延迟的合法反馈。
- 通用延迟补偿（PDC）未实现：并行路径延迟不一致时只诊断，不对齐。
- Limiter 是采样峰值限制器，不保证 dBTP；母带反馈会测量并验证 True Peak。
- WAV 仅 float32；采样资产支持 PCM16/float32。
