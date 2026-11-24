# Phase 2 架构审查与实施方案（MPL V0.2.0）

日期：2026-11-25

## 1. 审查范围与证据

审查对象：MPL Core V0.1.0 的 Production IR / Temporal IR / AssetBinding / ParameterRegistry，以及 `F:\Soundboxer` 的音频引擎、AudioGraph、GraphCompiler、OfflineRenderer、ProjectModel、插件与导出路径。

SoundBoxer 事实（均可在源码中核对）：

| 能力 | 位置 | 结论 |
| --- | --- | --- |
| 低层音频图编译 | `src/dsp/graph_compiler/GraphCompiler.h`：`GraphCompiler::Compile(const AudioGraph&, Core::MemoryPool&)` → `CompiledGraph{RenderOp...}`，拓扑排序与环检测 | 可直接复用 |
| 音频引擎 process loop | `src/engine/audio/AudioEngine.h`：`StartEngine()`、`StartTransport()`、`BeginExclusiveOfflineRender()`、`EndExclusiveOfflineRender()` | 可复用，但需要工程与设备上下文 |
| 离线渲染 | `src/engine/audio/OfflineRenderer.h`：`RenderSource/RenderTrack/RenderProject(const ProjectModel&, audioCache, RenderConfig, outputPath)`，`RenderConfig{sampleRate, blockSize, tailSeconds, ExportFormat}` | 可复用，但强耦合 ProjectModel + 音频缓存 + 插件实例 |
| WAV/导出格式 | `OfflineRenderer.h` 的 `RenderResult`，`src/controller/ExportController.cpp` 的格式处理 | 可复用/可参考 |
| 工程模型 | `src/engine/project/ProjectModel.h`：trackTree、Midi/AudioClip、AssetType、MixerState、AutomationClip、AudioObject、FrozenRenderCache 等 | 可复用，但模型远大于 MPL Production IR |
| 图/节点 | `src/dsp/nodes/AudioNode.h`，`AudioGraph` 节点/连接 | 可复用 |
| 插件与音源 | `src/dsp/plugins`（VST3/CLAP、SandboxIPC）、`IAudioSource`、Sampler/Synth | 需要宿主初始化和资产/插件解析 |
| 构建 | 根 `build.bat`，MSVC + C++20 + Qt 6.8 QML，分层 static libs；引擎/DSP 为 L3/L2 | 可链接，但完整应用依赖大量 Qt/第三方库 |

**未发现**独立的 headless 渲染 CLI 目标；`OfflineRenderer` 的调用者位于 Controller/App 层，并依赖 Qt 工程上下文、插件扫描与音源缓存。故不能假设“把 Production IR 交给 OfflineRenderer 即可渲染”。

## 2. 复用判定

### 可直接复用

- `GraphCompiler` 与 `CompiledGraph` 的图编译思想/实现；
- `AudioNode`/`AudioGraph` 节点接口；
- `OfflineRenderer` 的离线渲染、尾音、采样率/BlockSize、WAV 导出能力；
- `TempoMap` 的 PPQ↔Sample↔BBT 换算逻辑（仅作参考/适配）；
- `ProjectModel` 的轨道、Clip、资产、Mixer、Automation 数据模型。

### 需要适配

- MPL Production IR → SoundBoxer `ProjectModel`：
  - TrackIR → TrackTree/MixerChannel/Track；
  - ClipIR → MIDI Clip（音符事件）或 Audio Clip；
  - AssetSlot/AssetBinding → `IAudioSource`/Instrument 插件实例；
  - AutomationBinding → ProjectModel 自动化/参数事件；
  - MPL `InstanceId`/`ObjectId` ↔ SoundBoxer Track/Clip/Asset ID 映射表；
  - MPL 时间（全音符有理数）→ SoundBoxer PPQ/采样。
- 插件/音源 bootstrap：SoundBoxer 渲染依赖已实例化的插件与音频缓存；MPL 当前只有符号 `AssetBinding`，需要资产解析器与插件宿主桥。
- 离线渲染上下文：需要构造 ProjectModel、AudioCache、Mixer、TempoMap，或新增 headless 入口调用 `OfflineRenderer`。

### 需要新增

- 后端无关的 **Audio Execution IR / 执行计划**（V0.2.0-alpha）；
- 可独立测试的 **Reference Audio Backend**（振荡器/ADSR/WAV/复音/混音/自动化/WAV 导出）；
- **AssetResolver** 接口与缺失诊断、版本、绑定固化；
- **MPL Audio Render 诊断**并与 Core/Frontend 统一输出；
- **headless `mplc build` 音频路径**；
- SoundBoxer Adapter（V0.2.0-beta）与身份映射表。

## 3. 里程碑

### V0.2.0-alpha — Reference Audio Backend（本轮实现）

- Audio Execution Plan：Track/Note/ControlBinding/AssetResolution。
- 采样精确的事件位置、Musical→Physical→Sample 转换。
- 振荡器（sine/triangle/saw/square/noise）、ADSR、WAV 采样、复音。
- Gain/Pan、多轨混合、基础自动化（Block 内线性插值）。
- WAV float32 导出（48k/stereo/256 默认，可配置）。
- 资产解析：内建符号音色 + WAV 文件；缺失/版本/替换测试。
- 统一诊断与 `mplc build --json`。
- 参考 MPL 音频程序与 WAV。

### V0.2.0-beta — SoundBoxer Adapter

- 新 headless 工具/入口（或 SoundBoxer 侧 adapter 库），构造最小 ProjectModel。
- Production IR → TrackTree/MIDI Clip/Audio Clip/Mixer/Automation 映射。
- 资产桥：AssetBinding → IAudioSource/插件实例；插件加载失败必须诊断。
- 调用 `OfflineRenderer::RenderProject`，保留 MPL↔SoundBoxer 身份映射。
- 与 Reference Backend 对同一案例做轨道/事件级一致性对比。

### V0.2.0 — Basic Production

- 多轨多音色、表情、自动化、母带 Gain、尾音与 rendered extent 定稿。
- 工程导出/回读、资产替换、诊断与黄金验证。
- 完整生产案例与开发报告。

## 4. 本轮不实现

LLM/Agent、情绪识别、Taste 学习、高级生成算子、约束求解、完整空间音频、用户 DSP、大规模语法扩展。SoundBoxer 完整双向同步留到 beta/后续。

## 5. 成功标准

MPL 源程序经 Music/Temporal/Production IR，由 Reference Backend 渲染出确定、可复现的 WAV；事件位置、复音、ADSR、Gain/Pan、自动化、尾音、资产替换与缺失诊断均有测试。SoundBoxer 集成以真实接口证据推进，不伪造 headless 能力。
