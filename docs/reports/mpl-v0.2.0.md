# MPL V0.2.0 开发报告 — Audio Execution & Music Production

日期：2026-11-25

范围：V0.2.0-alpha Reference Audio Backend

结论：完成 `MPL Source → Music/Temporal IR → Production IR → Audio Execution Plan → Reference Audio Backend → WAV` 闭环。MPL 源程序已实际渲染出 48 kHz / stereo / float32 WAV；111 项测试通过，新增音频数值黄金测试。

SoundBoxer 集成（V0.2.0-beta）本轮未实现，已完成基于真实源码的架构审查与适配方案。

---

## 1. 架构审查

证据与结论见 `docs/reports/phase2-architecture-review.md`：

- SoundBoxer 有 `GraphCompiler::Compile(AudioGraph, MemoryPool)`、`AudioEngine::BeginExclusiveOfflineRender()`、`OfflineRenderer::RenderProject(ProjectModel, audioCache, RenderConfig, path)`、`ProjectModel`、插件宿主与 WAV 导出。
- 可直接复用：GraphCompiler/AudioGraph、OfflineRenderer、ProjectModel、TempoMap、插件/音源基础设施。
- 需要适配：Production IR → ProjectModel/AudioGraph，AssetBinding → IAudioSource/插件，MPL 时间/身份 → SoundBoxer PPQ/Track/Clip。
- 需要新增：headless 渲染入口、资产生命周期、MPL↔SoundBoxer 身份映射。
- 未发现独立 headless 渲染 CLI；`OfflineRenderer` 依赖工程上下文与插件实例，因此不能直接把 Production IR 交给它。

本轮据此先实现 Reference Backend，避免无依据伪造 SoundBoxer 接口。

---

## 2. 已实现

### 2.1 Audio Execution Plan

- `AudioRenderConfig{sampleRate, blockSize, channels, tailSeconds}`。
- `TrackPlan{InstrumentAsset, PlannedNote[], ControlBindingPlan[]}`。
- 采样精确的 Note On/Off、力度、逐音符 pan。
- Tempo map 分段换算：Musical whole notes → seconds → samples，`llround` 舍入，至少 1 sample。
- 资产在计划构建时解析并固化 id/version。
- 三类时长：Section 排程、Musical Content、Rendered Audio Extent。

### 2.2 Reference Audio Backend

- 振荡器：sine / triangle / saw / square / deterministic noise。
- ADSR：线性 attack/decay，sustain，release 从 Note Off 实际电平开始。
- WAV 采样：PCM16 / float32，按音高与采样率比重采样。
- 复音、多轨混合、Track Gain/Pan、逐音符 Pan、Master Volume。
- 自动化：Replace/Add/Multiply；Block 边界求值，Block 内线性插值。
- 线性自动化在不同 Block Size（64/256/1024）下样本一致。
- float32 WAV 导出；尾音包含 release 与 `tailSeconds`。
- 缺失资产、未知音色、WAV 错误、无映射参数、不支持自动化均产生诊断。

### 2.3 前端与 CLI

- `export wav("...")`，支持同一程序多个导出。
- `mplc build --sample-rate/--block-size/--channels/--tail`。
- `mplc build --json` 输出前端（带 span）与 Core/音频（无 span）的统一诊断 JSON。
- 参考音频程序：`examples/audio.mpl`。

---

## 3. 验证

### 3.1 测试

- **111/111 测试通过**（Debug 与 Release）。
- CTest 1/1 通过。
- 新增音频测试：
  - 音高→频率（A4=440）；
  - 采样位置与 BPM 映射；
  - release/tail 精确帧数；
  - 复音、Gain、Pan；
  - ADSR 形状与 release；
  - 自动化 Block Size 一致性；
  - 多轨线性混合；
  - 音色替换（sine/saw）与结构不变；
  - 资产缺失/能力诊断；
  - WAV 采样文件播放；
  - WAV float32 读写 round-trip；
  - Audio Example 渲染与数值黄金。
- 保留 V0.1.0 全部前端/IR/MIDI 测试。

### 3.2 音频产物

`examples/audio.mpl`：

| 项目 | 数值 |
| --- | --- |
| 轨道 / Clip / 音符 | 4 / 5 / 106 |
| 曲线 / 自动化 | 3 / 2 |
| 内容时长 | 8 小节 / 12 秒 @160 |
| 渲染配置 | 48000 Hz、stereo、float32、block 256、tail 1.0 |
| WAV | 672,000 frames / 14.0 s / 5,376,044 bytes |
| Peak | 0.74 |

黄金音频统计（8000 Hz 测试配置）：

```json
{
  "sampleRate": 8000,
  "channels": 2,
  "frames": 104800,
  "peak": 0.634806,
  "rms": 0.139374,
  "hash": "89508503e4ca5cd7"
}
```

---

## 4. 交付物

| 交付物 | 路径 |
| --- | --- |
| 音频执行规范 | `docs/audio-execution-spec.md` |
| Phase 2 架构审查/方案 | `docs/reports/phase2-architecture-review.md` |
| 参考音频程序 | `examples/audio.mpl` |
| 实际 WAV | `build/audio-full/audio.wav` |
| 音频 IR/Trace | `build/audio-full/audio.*.ir`、`audio.trace.json` |
| ADR | `ADR-0011`（Reference Backend）、`ADR-0012`（时间/自动化/尾音）、`ADR-0013`（资产解析） |
| 测试 | `tests/test_audio.cpp`、`tests/golden/audio-reference.json` |

---

## 5. 已知局限

1. SoundBoxer Adapter/headless 渲染尚未实现（beta）。
2. 参考后端音质有限：无抗混叠、无滤波器、无高级采样插值。
3. 音频图只有 Track → Master，无 Send/Return/侧链。
4. 非线性自动化按 Block 边界采样，存在 Block Size 近似；线性曲线一致。
5. 只支持 volume/pan 自动化映射；其他参数诊断。
6. WAV 只导出 float32；采样资产支持 PCM16/float32。
7. 无实时调制、MPE、微分音、空间音频、插件宿主。
8. Section 越界仍是警告；音频按内容与尾音渲染，不按 section 截断。
9. 每次 build 重新渲染，无音频缓存；无跨构建增量。
10. 音频资产版本已固化在计划中，但尚未写入可回读的工程文件。

---

## 6. 下一阶段（V0.2.0-beta）

1. 在 SoundBoxer 侧建立 headless adapter（或独立 CLI），构造最小 ProjectModel/AudioCache/Mixer。
2. Production IR → ProjectModel/AudioGraph 映射：Track、MIDI/Audio Clip、Mixer、Automation、资产。
3. 复用 `OfflineRenderer::RenderProject` 输出 WAV；保留 MPL↔SoundBoxer ID 映射。
4. 与 Reference Backend 对同一案例做事件/轨道级对比，记录差异。
5. 资产桥：AssetResolver → IAudioSource/插件；插件加载失败诊断。

---

## 7. 验收对照

| 要求 | 状态 |
| --- | --- |
| 架构审查（SoundBoxer/Production/Temporal/Assets） | 完成，含源码证据 |
| 音频执行边界与 Execution IR | 完成 |
| 振荡器/ADSR/WAV/复音/Gain/Pan/多轨/自动化/WAV 导出 | 完成 |
| Musical→Physical→Sample、舍入、采样精确事件 | 完成 |
| Block 内自动化与 Block Size 验证 | 完成（线性曲线一致） |
| 资产解析、版本、缺失诊断、音色替换 | 完成 |
| Section/Content/Rendered Extent | 完成 |
| 统一诊断与 `build --json` | 完成 |
| SoundBoxer Integration | 审查与方案完成，beta 实现未开始 |
| 音频测试与黄金 | 完成 |
| 完整 MPL 音频案例 | `examples/audio.mpl` + WAV |
| 范围限制（无 LLM/DSP/约束/空间音频） | 遵守 |

V0.2.0-alpha 的成功标准已满足：MPL 源程序能够正确驱动真实音频生产。
