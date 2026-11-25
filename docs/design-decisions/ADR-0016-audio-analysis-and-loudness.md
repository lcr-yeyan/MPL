# ADR-0016：音频分析、True Peak 与响度

状态：已采纳（V0.2.0）

## 背景

母带处理与生产反馈需要可重复的峰值、动态与响度测量。RMS 不能替代 LUFS；Sample Peak 不能替代 True Peak。

## 决策

1. Audio Analysis 是独立的只读模块，输入 AudioBuffer，输出结构化测量与诊断。
2. 指标：Sample Peak、RMS（跨全部声道样本）、Crest Factor、True Peak、Integrated Loudness、Max Short-term Loudness。
3. True Peak 使用 4 倍 polyphase Hann 窗 sinc（49 taps），每相归一化为单位直流，边界采用样本保持；结果取过采样峰值与 Sample Peak 的较大值。
4. Loudness 遵循 BS.1770 核心算法：
   - K 加权（高架 + 高通，按采样率推导原型）；
   - 400 ms 块、100 ms 跳步；
   - -70 LUFS 绝对门限 + -10 LU 相对门限；
   - `L = -0.691 + 10 log10(mean gated energy)`；
   - Short-term 为 3 s 窗口、100 ms 跳步，仅绝对门限。
5. 静音/短音频的响度定义为 `-inf`，JSON 写为 `null`；峰值/RMS/Crest 为 0。
6. NaN/Inf 视为 0 并报告 `analysis-non-finite`；Sample Peak > 1 仅设置 `clipped` 标志，不做裁剪。
7. 参考验证：-20 dBFS 997 Hz stereo 正弦读 -20 LUFS；fs/4、45°、0.5 幅度正弦的 True Peak 比 Sample Peak 高约 3 dB。

## 后果

- 分析与渲染解耦，可独立测试。
- RMS 与 LUFS 在 API、文档与测试中明确区分。
- True Peak 是工程级 4 倍过采样估计，不声称合规认证。
- 未来可替换为 ITU 系数表或其他参考实现，而不改变上层接口。
