# ADR-0014：信号处理图与基础 DSP

状态：已采纳（V0.2.0-beta）

## 背景

V0.2.0-alpha 的参考后端只有 Track → Master 两级求和，无法表达插入链、Bus/Send、Sidechain 与处理器参数自动化。Beta 需要一个最小但可验证的信号执行结构，同时不能引入 SoundBoxer 的 AudioGraph。

## 决策

1. 在 Audio Execution Plan 中引入 `SignalGraphPlan`：
   - 节点：Source、Mixer、Processor、Output；
   - 连接：`{from, to, gain, sidechain}`；
   - 节点声明输入/输出声道数，内部端口统一 stereo。
2. 允许的边：
   - Source → Processor/Mixer/Output（音频）；
   - Processor/Mixer → Processor/Mixer/Output（音频）；
   - Source → Processor（sidechain，仅 Compressor）。
3. 执行顺序由确定性拓扑排序给出（同层按节点 id 升序）。
4. 本阶段拒绝所有环路，诊断 `graph-cycle`。规范区分“普通循环依赖”与“未来由 Delay 显式声明最小延迟的合法反馈”；后者不实现。
5. 声道兼容性：stereo 输出不得连接 mono 输入；mono 源在 Source 节点升为 stereo。
6. 处理器状态在渲染开始 `prepare/reset`，块间保持，重渲染从头复位。
7. 基础 DSP 集合：Gain、Pan、Mixer、Biquad、Compressor、Delay、Reverb。
   - Biquad：RBJ 系数，5 ms 截止频率平滑；
   - Compressor：峰值检测、软起音/释放、可选 sidechain；
   - Delay/Reverb：有限尾音，参与 `processorTail` 计算。
8. 参数使用 `ParameterRamp` 在块内线性插值；处理器逐样本读取所需参数，避免把 Block Size 差异放大为可听误差。

## 后果

- 混音语义可以结构化验证，而不是不可检查的字符串命令。
- Reference Backend 成为信号图语义的测试 oracle。
- 图执行是离线、非实时的；未来可将同一计划映射到其它后端。
- 反馈、audio-rate modulation、插件宿主不在本阶段范围。
