# Phase 2-beta 独立运行时与信号计算架构设计

日期：2026-11-25

## 1. 回归审查口径

| 测试文件 | 数量 |
| --- | --- |
| test_core | 25 |
| test_ir | 6 |
| test_lowering | 8 |
| test_semantics | 13 |
| test_temporal | 16 |
| test_backend | 9 |
| test_golden | 6 |
| **V0.0.3 / 旧 Core 小计** | **83** |
| test_frontend | 15 |
| **V0.1.0 小计** | **98** |
| test_audio | 13 |
| **V0.2.0-alpha 小计** | **111** |
| test_signal | 20 |
| **V0.2.0-beta 实现后总计** | **131** |

结论：V0.1.0 的 98 项测试完整保留；V0.2.0 报告中的“83 项既有测试”是把 V0.0.3 基线误当成 V0.1.0 全部测试，属于口径错误。Beta 实现后总计 131 项 = 83 + 15 + 13 + 20，无测试被删除。

实现状态：本文件的信号图、Backend Contract、DSP、混音语法、自动化执行与终止策略均已落地；实现与验证结果见 `docs/reports/mpl-v0.2.0-beta.md`。

## 2. 战略边界

- 取消 beta 必须接入 SoundBoxer 的要求。
- MPL 不引入 SoundBoxer ProjectModel/AudioEngine/AudioGraph/插件宿主。
- 保留既有 SoundBoxer 架构审查，作为未来可选 Adapter 参考。
- 本阶段构建独立、无界面、可脱离 DAW 运行的音乐生产执行环境。

## 3. 可复用与必要修改

### 直接复用

- Music/Temporal/Production IR；
- Audio Execution Plan 的事件/资产/自动化模型；
- Reference Backend 的振荡器、ADSR、采样、WAV、复音；
- AssetResolver、WAV I/O、统一诊断；
- TempoMap / AudioClock。

### 必要修改

- Production IR 增加 MixSpec（processor definitions、track strips、buses、master、processor automation）与 logicalExtent。
- Audio Execution Plan 增加 SignalGraphPlan、logical/content/processor/render extent。
- Reference Backend 从“直接 track→master”升级为“按 Signal Graph 拓扑执行”。
- 语言增加 processor / bus / master / insert / send / sidechain / processor automation。
- 新增 BackendCapabilities 与 UnsupportedPolicy。

### 不新增

- 不复制 SoundBoxer 的工程/插件模型；
- 不建立 GUI/设备管理/插件宿主；
- 不实现通用 DSP 语言。

## 4. Backend Contract

```text
BackendCapabilities
├── sampleAccurateEvents
├── processorAutomation
├── sidechain
├── busesAndSends
├── silenceTermination
├── supportedProcessorKinds[]
└── maxChannels

AudioRenderConfig
├── sampleRate / blockSize / channels
├── tailSeconds / maxTailSeconds
├── logicalExtent handling
└── termination: fixed | silence-threshold

AudioRenderResult
├── AudioBuffer
├── diagnostic[]
├── extent report (logical/content/processor/render)
└── backend identity/version
```

不支持语义策略：

- **Strict**：遇到不支持的处理器/参数/路由 → error，不渲染产物。
- **Approximate**：可用近似或旁路，产生 warning。
- **Ignore**：用户显式允许忽略，仍记录 note 级诊断，不静默。

## 5. 信号计算模型

类型：

- Audio Stream：交错 float 立体声块；
- Event Stream：PlannedNote（采样精确）；
- Control Stream：参数曲线/自动化；
- Audio Port / Control Port；
- Processor：有状态音频处理单元；
- Bus：多输入求和后处理；
- Connection：音频边，带增益；
- Sidechain Input：压缩器等处理器的键输入。

SignalGraph：

```text
TrackSource → Processor… → Track Gain → Track Pan → MasterMixer
                               └── send gain → BusMixer → Bus Processor… → Bus Gain/Pan → MasterMixer
MasterMixer → Master Processor… → Master Gain → Output
Sidechain: TrackSource → Compressor.key
```

规则：

- 图节点按拓扑序执行；所有反馈回路本阶段拒绝。
- 普通循环依赖 → `graph-cycle` 错误。
- 未来带明确延迟的合法反馈必须由 Delay 节点声明最小延迟并显式标记；本阶段不实现。
- 所有内部端口为 Stereo；mono 源在 Source 节点升为 stereo；stereo→mono 连接拒绝。
- 处理器状态在渲染开始时 `prepare/reset`；块间保持，重新渲染从头复位。
- 处理器链顺序 = 声明顺序。

## 6. DSP

| 处理器 | 参数 | 语义 |
| --- | --- | --- |
| Gain | gain dB | 线性增益 |
| Pan | pan -1..1 | 等功率声像/平衡 |
| Mix | 多输入 | 按连接增益求和 |
| Biquad | mode, frequency, q, gain | RBJ lowpass/highpass/peaking/lowshelf/highshelf |
| Compressor | threshold, ratio, attack, release, makeup | 峰值检测、软起音/释放、可选 sidechain |
| Delay | time, feedback, mix | 立体声延迟线，时间平滑，有限尾音 |
| Reverb | room, damping, mix | 小型 Freeverb 风格 comb/allpass，有限尾音 |

数值优先：参数单位、范围、默认值、状态复位、尾音可解释。第三方库可独立依赖，但不引入 SoundBoxer。

## 7. 混音语义与文本语法

语义：

- Track Insert：按顺序串入。
- Send：后推子发送到 Bus；Send level 为 0..1。
- Bus：求和所有输入 → 插入链 → Bus Gain/Pan → Master。
- Master：所有 Track/Bus 输出求和 → 插入链 → Master Gain → Output。
- Sidechain：`sidechain <processor> from <track>`，键信号取源轨 Source 输出（pre-insert）。
- Processor Parameter Automation：Musical 域曲线，按参数描述符范围校验。

语法（本轮实现）：

```text
processor Eq = biquad {
  mode = peaking;
  frequency = 1200 Hz;
  q = 1.0;
  gain = 3 dB;
};

processor Comp = compressor {
  threshold = -18 dB;
  ratio = 4;
  attack = 10 ms;
  release = 120 ms;
  makeup = 3 dB;
};

bus Reverb {
  insert Hall;
  gain = -3 dB;
}

track Piano {
  asset = GrandPiano;
  role = melodic;
  gain = -2 dB;
  pan = 0;
  insert Eq;
  insert Comp;
  send 0.25 to Reverb;
  sidechain Comp from Drums;
}

master {
  insert MasterComp;
  gain = -1 dB;
}

automation Piano.Eq.frequency = FilterCurve;
automation Reverb.Hall.mix = SendCurve;
automation master.MasterComp.threshold = MasterCurve;
```

- 2 段 automation 目标仍表示 track/master 的 volume/pan。
- 3 段目标表示 `<target>.<processor>.<parameter>`。
- Processor 定义是唯一定义；每次 insert 在图中创建独立实例与状态。

## 8. 自动化执行

- 事件：采样精确。
- Track volume/pan：块边界求值，块内线性插值。
- Processor 参数：块边界求值，块内线性插值；滤波器截止频率使用 5 ms 一阶平滑后更新系数。
- 非线性曲线：块边界采样，Block Size 相关；规范记录近似误差。
- Step/Keyframe/Repeat 边界：TemporalIR 求值语义不变；不连续点按段右连续。
- 参数范围：处理器描述符校验；超界夹紧/诊断。
- 未来 audio-rate modulation 以逐样本求值接口保留，本阶段不实现。

## 9. 渲染终止

- Logical Extent：arrangement/逻辑留白，不得被截断。
- Content Extent：最后音符结束。
- Processor Tail：Delay/Reverb 等有限状态衰减长度。
- Render Extent：

```text
base = max(logicalExtent, contentExtent + processorTail)
renderExtent = min(base + tailSeconds, base + maxTailSeconds)
```

- `Fixed`：按 renderExtent 渲染。
- `SilenceThreshold`：在逻辑/内容边界之后可裁剪低于阈值的尾部，但不得早于 logicalExtent。
- 无限状态处理器必须被 `maxTailSeconds` 截断并产生诊断。

## 10. 测试与交付

新增：图拓扑/非法连接、声道、EQ 响应、压缩器动态、Delay 位置、Reverb 尾音、Bus、Send/Return、Sidechain、处理器自动化、不连续曲线、Block Size 误差、状态复位、逻辑留白、终止、能力诊断。

Production Demo：多段落、多轨多音色、表情、音量与滤波自动化、EQ、Compressor、Reverb Send、Sidechain、Master、WAV，完全独立于 SoundBoxer。

不实现：Adapter、GUI、实时设备、完整插件宿主、AI、情绪/Taste、空间音频、通用 DSP 语言。
