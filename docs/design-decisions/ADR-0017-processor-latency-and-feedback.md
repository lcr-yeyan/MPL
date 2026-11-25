# ADR-0017：处理器延迟、Limiter 与生产反馈

状态：已采纳（V0.2.0）

## 背景

母带 Limiter 需要前视，因而引入内部处理延迟；Delay/Reverb 引入的是尾音而不是延迟。两者若混同，会破坏时间对齐或错误延长渲染。另一方面，响度目标需要跨多次渲染的反馈，但不能把反馈塞进 SignalGraph。

## 决策

1. 明确区分：
   - Processor Latency：为保证处理正确性引入的内部延迟，需要补偿；
   - Processor Tail：输入结束后继续发声的长度，只影响渲染长度；
   - 艺术性 Delay time：不属于 Processor Latency，不补偿。
2. `AudioProcessor` 提供 `latencySeconds()`；计划层为每个 Processor 节点记录延迟。
3. 执行计划按主信号路径累计延迟：
   - 所有路径延迟一致时，渲染器裁掉前导延迟，保持音乐位置；
   - 同一节点输入路径延迟不一致时产生 `audio-latency-misaligned`；
   - 本阶段不实现通用 PDC。
4. Limiter：
   - 参数 ceiling（dB）、attack（秒，前视）、release（秒）；
   - attack 同时定义内部延迟，且禁止自动化；
   - 采样峰值检测 + 前视 + 平滑增益 + 最终样本 clamp；
   - 只保证 Sample Peak ceiling，不保证 True Peak，不宣称 dBTP Limiter。
5. Production Feedback：
   - 独立于 SignalGraph，执行 Render → Analyze → Adjust → Render → Verify；
   - 只调整 master gain 与 limiter ceiling；
   - 定义目标 LUFS、容差、dBTP ceiling、最大迭代次数；
   - 失败必须诊断：`mastering-silent`、`mastering-unsatisfiable`、`mastering-peak-exceeded`、`mastering-max-iterations`；
   - 每轮记录参数与分析，最终输出可审计 Trace。

## 后果

- 母带 Limiter 不会意外平移作品时间。
- 并行路径未对齐时不会静默产生相位问题，而是明确诊断。
- 响度目标求解可复现、可失败、可审计。
- True Peak 限制留待未来的真正 4 倍处理 Limiter；当前反馈负责验证，不假装保证。
