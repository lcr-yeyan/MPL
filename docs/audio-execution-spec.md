# MPL 音频执行规范（V0.2.0-alpha）

## 1. 目标

定义从 Production IR 到实际音频的稳定语义边界，并提供一个可独立测试的 Reference Audio Backend。它验证 MPL 音乐程序能否正确驱动音频生产，不试图替代 SoundBoxer 的完整 DSP/DAW 能力。

## 2. 执行边界

| 概念 | 本阶段实现 |
| --- | --- |
| Event Stream | `PlannedNote`：采样精确的 Note On/Off、力度、声像 |
| Control Stream | `ControlBindingPlan`：按音乐时间求值的音量/声像自动化 |
| Audio Stream | 分块渲染的交错 float 立体声/单声道缓冲 |
| Stateful Processor | Voice：振荡器/采样播放器 + ADSR 状态机 |
| Instrument | `InstrumentAsset`：波形、包络、增益、可选 WAV 采样 |
| Audio Clip | Production IR 的 ClipIR，展开为 PlannedNote |
| Audio Connection | Track → Master 两级求和（参考后端最小路由） |
| Control Binding | 后端无关 `AutomationBinding` 在音频层映射为音量/声像 |
| Output Bus | Master stereo bus → WAV |

`AudioExecutionPlan` 是 Reference Backend 的执行计划，不取代 SoundBoxer 的 AudioGraph。Beta 阶段再把 Production IR 映射到 SoundBoxer ProjectModel/AudioGraph。

## 3. 时间与采样

### 3.1 转换链

```text
MusicalTime (whole notes, Rational)
  → PhysicalTime (seconds, tempo map)
  → SampleTime (int64 frames)
```

- Tempo map 为分段常速；`AudioClock` 提供 `secondsAtWhole` / `wholeAtSeconds`。
- 音符起点/终点：`llround(seconds × sampleRate)`。
- 起点不小于 0；终点至少为起点 + 1 sample。
- 事件位置与 Block Size 无关；Block 只影响处理循环。

### 3.2 三种时长

| 名称 | 定义 | 是否截断音频 |
| --- | --- | --- |
| Section Logical Extent | 前端 arrangement 的段落排程长度 | 否 |
| Musical Content Extent | 所有 clip 内容/音符结束的最大值 | 否 |
| Rendered Audio Extent | `max(音符结束 + ADSR release) + tailSeconds` | 是（渲染长度） |

Section 越界只产生警告；音频不会在 section 边界截断。音符结束、段落结束、尾音结束三者不混同。

## 4. Audio Execution Plan

```text
AudioExecutionPlan
├── config (sampleRate, blockSize, channels, tailSeconds)
├── tempo segments
├── TrackPlan[]
│   ├── InstrumentAsset (resolved, versioned)
│   ├── PlannedNote[] (sample positions, velocity, optional pan)
│   └── ControlBindingPlan[] (volume/pan)
└── masterControls
```

- 资产在每个 track 的 `main` AssetBinding 上解析一次并固化到计划（id/version）。
- 同一音乐程序可替换 Resolver，而不改变音符/时间结构（测试覆盖）。
- 缺失资产/未知音色 → `audio-missing-asset` / `audio-unknown-asset` 错误，不静默静音。
- 演奏表情在计划构建时求值：velocity、timing、gate、逐音符 pan；音频后端支持逐音符 pan。

## 5. 自动化

- 支持 track/master 的 `volume`、`pan`，Musical 域。
- 组合模式沿用 Core：Replace 为基值，Add 求和，Multiply 求积。
- 每个 Block 计算起点/终点音乐位置的控制值，Block 内线性插值；音符事件仍是采样精确。
- 线性自动化在 64/256/1024 Block Size 下产生一致样本（测试覆盖）。
- 非线性曲线在 Block 边界采样；未来可切换为逐样本/audio-rate 求值。
- 不支持的参数（如 modulation）或 clip 作用域 → `audio-unsupported-parameter` / `audio-unsupported-automation` 警告。

## 6. Reference Backend

默认配置：

```text
sampleRate = 48000
blockSize  = 256
channels   = 2
format     = 32-bit float WAV
tailSeconds = 1.0
```

DSP：

- 振荡器：sine / triangle / saw / square / deterministic noise。
- ADSR：线性 attack/decay，sustain，release；release 从 Note Off 时的实际电平开始。
- 采样播放：PCM16 / float32 WAV，线性/最近点读取，按音高比与采样率比重采样。
- 复音：每轨可同时多 Voice。
- Gain/Pan：轨道增益、轨道声像、逐音符声像、Master Volume；等功率声像。
- 混合：轨道求和到 Master；浮点输出，不做硬限幅。
- 尾音：渲染长度包含 release 与 `tailSeconds`。

## 7. 诊断

| 错误码 | 含义 |
| --- | --- |
| `audio-missing-binding` | 轨道没有资产绑定 |
| `audio-missing-asset` | 资产无法解析/文件缺失/WAV 读取失败 |
| `audio-unknown-asset` | 参考库没有对应的符号音色 |
| `audio-unsupported-parameter` | 参数无音频映射 |
| `audio-unsupported-automation` | 非 musical 域或 clip 作用域自动化 |
| `wav-read` / `wav-write` | WAV 解析/写入失败 |
| `audio-config` | 非法采样率/Block/声道配置 |

`mplc build --json` 输出前端（带 span）与 Core/音频（无 span）诊断的统一 JSON 数组。

## 8. 与 SoundBoxer 的边界

Reference Backend 不替代 SoundBoxer：

- SoundBoxer 复用点：`GraphCompiler`、`AudioGraph`、`OfflineRenderer`、ProjectModel、WAV 导出、插件宿主。
- Beta 适配：Production IR → ProjectModel/AudioGraph；AssetBinding → IAudioSource/插件；自动化 → 工程参数；ID 映射。
- 当前 SoundBoxer 没有独立 headless 渲染 CLI；`OfflineRenderer` 依赖 ProjectModel、音频缓存与插件实例，完整集成需要构造工程上下文。
- 在 adapter 完成前，MPL 不声称支持 SoundBoxer 渲染。

## 9. 已知限制

- 参考后端不是商业级音源；振荡器无抗混叠。
- 采样播放为最近点/简单重采样；无循环、切片、速度同步。
- 自动化 Block 边界采样对非线性曲线有 Block 依赖；线性曲线一致。
- 无实时调制、MPE、微分音、空间音频、侧链、插件宿主。
- WAV 仅 float32；采样资产支持 PCM16/float32。
- 音频图只有 Track → Master 两级；无 Send/Return。
