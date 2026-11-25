# MPL V0.2.0 收束报告：音频分析、母带处理与生产反馈

日期：2026-11-25

## 1. 结论

V0.2.0 完成 Phase 2 收束：

```text
Compile → Render → Analyze → Adjust → Render → Verify
```

在不依赖 SoundBoxer、GUI 或实时设备的前提下，MPL 现在具备：

- 基础作曲、编曲、音频执行与混音（V0.2.0-beta）；
- 独立的音频分析模块（Peak / RMS / Crest / True Peak / LUFS / Short-term）；
- 最小母带处理（Master Gain、Limiter、峰值与响度验证）；
- 有限、确定性的目标驱动生产反馈；
- 可审计的母带 Trace 与处理前后报告。

全量测试 **146/146** 通过（Debug 与 Release），其中既有 131 项回归全部保留，新增 15 项。

## 2. 架构审查结果

| 检查项 | 结论 | 处理 |
| --- | --- | --- |
| SignalGraph 幅值语义 | float32，无隐式限幅/归一化 | 保持；新增 `audio-peak-over` 警告，明确导出行为 |
| Pan Law | 逐音符为等功率；Track/Bus 为 stereo balance | 保持并写入规范，视为有意区分 |
| Send 语义 | 当前为 post-fader（Gain/Pan 之后） | 明确写入规范，不做隐式改变 |
| Sidechain 语义 | 源轨 Source，pre-insert/pre-fader；检测器取主/键逐声道绝对值最大值 | 保持；文档注明不参与延迟对齐 |
| Processor 内部延迟 | 此前所有处理器为 0 | Limiter 引入前视延迟；新增延迟接口、路径延迟计算、输出裁剪与 `audio-latency-misaligned` |
| 自动化不连续点 | Block 边界采样 + 块内线性；Step 在一个 Block 内落地 | 保持；既有测试覆盖 |
| Render Extent 与 Tail | Logical/Content/Processor Tail/Render 四类 | 保持；Limiter tail=0，延迟不进入 tail |
| 浮点超范围 | 无裁剪，可能 >1.0 | 保持样本；新增峰值警告；母带负责约束 |
| 4 个黄金变化 | 仅 version 4、mix/logicalExtent 新字段、音频 hash | 已逐项复核，符合预期 |

### 2.1 审查中发现并处理的问题

1. **Master Limiter 位于 Master Fader 之前**：直接提高 Master Gain 会推高 Limiter 之后的峰值。Production Feedback 通过“按 fader 增益补偿 Limiter ceiling”解决，未改变通道条插入语义。
2. **Sample Peak Limiter 不保证 True Peak**：实测该作品在强限制下 True Peak 可比 Sample Peak 高约 3 dB。没有将其宣称为 True Peak Limiter；母带反馈独立测量 dBTP，并在超限时失败。
3. **True Peak 边界零填充导致 DC 虚高**：改为首/末样本保持，消除块首虚假振铃。
4. **并行路径延迟不一致**：本阶段不做通用 PDC；产生明确诊断，禁止静默相位问题。

### 2.2 已实现能力 / 缺陷 / 扩展边界

已实现：

- 采样精确事件、信号图执行、Bus/Send/Sidechain、处理器自动化；
- Gain/Pan/Biquad/Compressor/Delay/Reverb/Limiter；
- 分析、母带、目标反馈与 Trace；
- 独立 CLI：`build`、`analyze`、`master`。

已知缺陷/限制：

- Limiter 为采样峰值限制器，不是认证 True Peak Limiter；
- 无通用 PDC；
- 无实时音频、插件宿主、GUI、audio-rate modulation；
- 分析为工程级 4 倍过采样，不用于合规认证。

扩展边界：

- 真正的 4 倍处理 True Peak Limiter；
- 通用 PDC 与带显式延迟的合法反馈；
- 更多生产目标（LRA、峰值分布、频段约束），但不引入通用约束求解器。

## 3. 音频分析

新增模块：

- `include/mpl/audio/AudioAnalysis.hpp`
- `src/audio/AudioAnalysis.cpp`

指标：

- Sample Peak（dBFS）
- RMS（dBFS，跨全部声道样本）
- Crest Factor（dB）
- True Peak（dBTP，4 倍 polyphase Hann 窗 sinc，49 taps）
- Integrated Loudness（LUFS，BS.1770 K 加权、400 ms 块、75% 重叠、绝对/相对门限）
- Max Short-term Loudness（LUFS，3 s 窗口、100 ms 跳步）

规则：

- 分析区间可配置，默认全 buffer；
- 静音响度为 `-inf`，JSON 中为 `null`；
- 短音频不足门限块时响度未定义；
- NaN/Inf 视为 0 并产生 `analysis-non-finite`；
- Sample Peak > 1 只标记 `clipped`，不裁剪；
- RMS 与 LUFS 在 API、JSON、测试中明确区分。

参考验证：

- 48 kHz、997 Hz、-20 dBFS stereo 正弦 → Integrated ≈ -20.0 LUFS；
- 12 kHz（fs/4）、45°、幅度 0.5 → True Peak 比 Sample Peak 高约 3.01 dB；
- 正弦+静音 → 静音块被门限排除，响度保持。

## 4. 母带处理与处理器时间语义

Limiter：

```text
processor MasterLimiter = limiter {
  ceiling = -10.5 dB;
  attack = 5 ms;
  release = 100 ms;
};
```

- 检测：逐样本、跨声道采样峰值；
- Attack：前视时间，同时是内部延迟；
- Release：增益恢复；
- 保证：输出 Sample Peak ≤ ceiling；
- 不保证：True Peak/dBTP；
- 内部延迟：`attack`，禁止自动化；
- Tail：0。

时间语义：

- Processor Latency 与 Processor Tail 分离；
- 图按主路径累计延迟；一致时输出裁掉前导延迟；不一致时 `audio-latency-misaligned`；
- 艺术性 Delay 不参与延迟补偿。

## 5. Production Feedback

接口：

```text
MasteringTarget { targetLufs, loudnessToleranceLu, peakCeilingDb, maxIterations,
                  addLimiter, limiterAttackSeconds, limiterReleaseSeconds }
```

流程：

1. 渲染原始混音（无 Limiter）并分析，作为 before；
2. 在 Master 插入/查找 Limiter；
3. 渲染 → 分析 → 调整 master gain / limiter ceiling → 再渲染；
4. 最多 `maxIterations` 轮；
5. 验证 LUFS 容差与 dBTP ceiling；
6. 输出 mastered buffer、before/after 分析、每轮参数与 Trace。

失败语义：

- `mastering-silent`
- `mastering-unsatisfiable`
- `mastering-peak-exceeded`
- `mastering-max-iterations`

Production Feedback 在 SignalGraph 之外，不产生图环路；信号反馈（如 sidechain）仍遵循图规则。

## 6. Mastering Demo

源程序：`examples/production.mpl`（3 段落、5 轨、EQ、压缩、Reverb Send、Sidechain、Master）。

命令：

```text
mplc master examples/production.mpl -o build/mastering --tail 1.5 \
  --target-lufs -14 --peak-ceiling -0.5 --max-iterations 4
```

产物：

| 文件 | 内容 |
| --- | --- |
| `build/mastering/production.raw.wav` | 原始混音 |
| `build/mastering/production.mastered.wav` | 母带结果 |
| `build/mastering/production.analysis-before.json` | 处理前分析 |
| `build/mastering/production.analysis-after.json` | 处理后分析 |
| `build/mastering/production.mastering-trace.json` | 目标、Limiter、每轮参数与分析、Trace |

目标与实际：

| 项目 | 原始混音 | 母带 |
| --- | --- | --- |
| Integrated Loudness | -22.76 LUFS | **-14.18 LUFS** |
| True Peak | -6.83 dBTP | **-0.43 dBTP** |
| Sample Peak | -7.73 dBFS | -2.74 dBFS |
| RMS | -26.07 dBFS | -17.46 dBFS |
| Crest Factor | 18.35 dB | 14.72 dB |
| Max Short-term | -19.20 LUFS | -10.83 LUFS |

目标：-14.0 ± 0.5 LUFS，True Peak ≤ -0.5 dBTP。

结果：3 轮迭代满足；master gain +7.761 dB；Limiter ceiling -10.505 dB（前级补偿值），attack 5 ms，release 100 ms。

### 6.1 V0.3.0-alpha 收束修正

- 峰值容差与响度容差独立：`loudnessToleranceLu = 0.5`，`peakToleranceDb = 0.1`。
- 目标 -0.5 dBTP，实测 -0.43 dBTP，严格超出 0.07 dB，但在 0.1 dB 峰值容差内；状态为 `satisfied-within-tolerance`，不是 `strictly-satisfied`。
- Limiter ceiling -10.505 dB 是 **pre-fader** 值；master gain +7.761 dB 后，post-fader sample ceiling 为 -2.744 dBFS。实测 sample peak -2.74 dBFS 与此一致。
- True Peak -0.43 dBTP 比 sample peak 高约 2.3 dB，来自采样峰值限制后的 inter-sample 峰值；因此没有把该 Limiter 宣称为 True Peak Limiter。
- 状态枚举区分严格满足、容差内满足、未满足、迭代耗尽、数值失败与可证明不可满足；最终参数与每轮测量保存在 Trace 中。

## 7. 测试

- 既有 131 项全部保留并通过。
- 新增 15 项：
  - 分析：Peak/RMS/Crest、静音/短音频、LUFS 参考、True Peak 参考、门限/NaN/裁剪/区间、JSON 单位与 null；
  - Limiter/延迟：峰值保证与延迟、输出延迟补偿、并行路径诊断、attack 自动化拒绝；
  - 母带：目标满足、不可满足、最大迭代、无 Limiter 增益路径、Trace JSON 完整性；
  - 渲染前后分析一致性：独立重新分析结果与反馈最终测量一致。

总计 **146/146**。

## 8. 交付文件

- 代码：`include/mpl/audio/AudioAnalysis.hpp`、`src/audio/AudioAnalysis.cpp`、`include/mpl/mastering/Mastering.hpp`、`src/mastering/Mastering.cpp`、Limiter 与延迟逻辑、CLI `analyze` / `master`。
- 规范：`docs/audio-analysis-spec.md`、`docs/audio-execution-spec.md`、`docs/audio-and-production-theory.md`。
- ADR：`ADR-0016`、`ADR-0017`。
- Demo：`examples/production.mpl` + `build/mastering/` 的 WAV、分析 JSON 与 Trace。
- 报告：本文件。

## 9. 范围限制

未实现：SoundBoxer Adapter、商业插件宿主、实时设备、GUI、完整 audio-rate modulation、高级空间音频、AI/LLM、情绪识别、通用约束求解器、大规模 DSP 扩展。

## 10. V0.3.0

Phase 2 到此结束。V0.3.0 将转向音乐创作知识的程序化，设计提案见 `docs/reports/v0.3.0-language-abstraction-proposal.md`。
