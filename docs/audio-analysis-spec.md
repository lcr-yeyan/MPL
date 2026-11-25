# MPL 音频分析规范（V0.2.0）

## 1. 定位

Audio Analysis 是独立、可测试的离线测量模块。它只读取 `AudioBuffer`，不修改信号、不依赖 SignalGraph，也不参与实时线程。它服务于母带处理、Production Feedback、测试和报告。

RMS 不是 LUFS。RMS 是未加权的样本均方根；LUFS 是经过 BS.1770 K 加权、分块与门限处理后的节目响度。两者不可互换。

## 2. 分析区间与声道

- 默认分析整个 buffer；`startFrame` 闭区间、`endFrame` 开区间，超出部分截断到 buffer 长度。
- 空区间产生 `analysis-empty-range` 警告，线性指标为 0，响度为 `-inf`。
- `frames` 是实际分析的帧数，`startFrame` 是实际起点。
- Peak：所有声道绝对值的最大值。
- RMS：所有声道全部样本的均方根，`sqrt(sum(x²) / (frames × channels))`。
- Crest Factor：`20 log10(peak / rms)`，静音定义为 0 dB。
- True Peak：各声道 4 倍过采样网格上的最大值，再与 Sample Peak 取较大者。
- Loudness：按 BS.1770 声道权重求和。MPL 支持 mono/stereo，两者权重均为 1.0。

## 3. 指标与单位

| 指标 | 单位 | 说明 |
| --- | --- | --- |
| Sample Peak | dBFS | 样本峰值，`20 log10(max abs)` |
| RMS | dBFS | 未加权均方根 |
| Crest Factor | dB | `peak / rms` |
| True Peak | dBTP | 4 倍过采样重建峰值 |
| Integrated Loudness | LUFS | 400 ms 分块 + 门限 |
| Max Short-term Loudness | LUFS | 3 s 滑动窗口最大值 |

线性值同时保留，便于数值比较。dB 指标在输入为静音时为 `-inf`，JSON 中写为 `null`。

## 4. True Peak

实现：

- 4 倍 polyphase FIR，49 taps，Hann 窗 sinc，每个相位归一化为单位直流增益。
- 每个相位只在整数延迟上卷积，取四相与 Sample Peak 的最大值。
- 边界使用首/末样本保持，不使用零填充，避免块首产生虚假振铃。

参考验证：

- 48 kHz、12 kHz（fs/4）、相位 45°、幅度 0.5 的正弦：Sample Peak ≈ 0.3536，True Peak ≈ 0.5，差值约 3.01 dB。
- 997 Hz 正弦：True Peak 与 Sample Peak 差值 < 0.2 dB。

限制：

- 不是 ITU-R BS.1770 附件中 48-tap 系数表的逐位复刻，而是同一 4 倍过采样思想的独立实现；用于工程判断与反馈，不用于合规认证。
- 4 倍网格仍是离散近似；极端瞬态可能略低于理论连续峰值。
- 边界保持会轻微改变最后/最初样本的真实峰值，报告需与区间一起解读。

## 5. 响度

实现遵循 ITU-R BS.1770 的核心算法：

1. K 加权：高架滤波 + 高通滤波，由标准原型参数按采样率推导；48 kHz 时与公开系数一致。
2. 400 ms 分块，100 ms 跳步（75% 重叠）。
3. 每块能量 `z = Σ_channels mean_square(channel)`。
4. 绝对门限：`L >= -70 LUFS`，等价于能量 `>= 10^((-70 + 0.691) / 10)`。
5. 相对门限：通过绝对门限块的平均能量 × 0.1（-10 LU）。
6. `L = -0.691 + 10 log10(mean energy of gated blocks)`。

Short-term：3 s 窗口、100 ms 跳步，只应用绝对门限，取最大值；不做相对门限。

静音、短音频与异常：

- 全静音：Peak/RMS/Crest/True Peak 为 0，Integrated/Short-term 为 `-inf`。
- 短于 400 ms：Integrated 为 `-inf`；短于 3 s：Short-term 为 `-inf`。
- 非有限样本（NaN/Inf）：视为 0，置 `nonFinite=true`，并产生 `analysis-non-finite` 警告。
- 采样率/过采样配置非法：`analysis-config` 错误。
- Sample Peak > 1.0：`clipped=true`；分析不裁剪、不归一化。

参考验证：

- 48 kHz、997 Hz、幅度 0.1（-20 dBFS）的 stereo 正弦：Integrated ≈ -20.0 LUFS，Short-term ≈ -20.0 LUFS。
- 5 s 正弦 + 5 s 静音：静音块被绝对门限排除，Integrated 仍约等于正弦段响度。
- 该结果与 EBU Tech 3341 的 -20 dBFS 参考音口径一致。

## 6. JSON

`mpl.audio-analysis` 固定字段：

```json
{
  "format": "mpl.audio-analysis",
  "version": 1,
  "sampleRate": 48000,
  "channels": 2,
  "frames": 1530000,
  "startFrame": 0,
  "endFrame": 1530000,
  "truePeakOversampling": 4,
  "samplePeak": 0.4108,
  "samplePeakDbfs": -7.727,
  "rms": 0.04969,
  "rmsDbfs": -26.075,
  "crestFactorDb": 18.348,
  "truePeak": 0.4554,
  "truePeakDbtp": -6.832,
  "integratedLoudnessLufs": -22.761,
  "maxShortTermLoudnessLufs": -19.202,
  "clipped": false,
  "nonFinite": false
}
```

未定义的 dB/LUFS 值为 `null`，保证 JSON 合法。

## 7. 明确边界

- 不提供 LRA、Momentary、频段分析、立体声宽度或感知指标。
- 不做音频修复、重采样或自动增益。
- 分析结果不写入 Production IR；它是渲染后的测量产物。
- True Peak 与 Loudness 的数值不构成商业合规认证。
