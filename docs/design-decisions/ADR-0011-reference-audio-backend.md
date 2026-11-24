# ADR-0011：Reference Audio Backend 作为音频语义可执行规范

状态：已采纳（V0.2.0-alpha）

## 背景

MPL 需要一个可独立验证的音频执行路径，以证明 Production IR/Temporal IR 能驱动真实 WAV；SoundBoxer 的 OfflineRenderer 存在，但耦合 ProjectModel、音源缓存和插件实例，且没有独立 headless CLI。

## 决策

1. 新增与 SoundBoxer 解耦的 Reference Audio Backend，作为音频语义的可执行规范与测试 oracle。
2. 新增 Audio Execution Plan，承载采样精确事件、资产解析结果和控制绑定。
3. Reference Backend 支持振荡器、ADSR、WAV 采样、复音、Gain/Pan、多轨混合、基础自动化与 float32 WAV。
4. 不把 Reference Backend 当作 SoundBoxer 的替代品；SoundBoxer 集成在 beta 阶段通过 Adapter 进行。
5. 缺失资产、未知参数、无法表达的自动化必须诊断，不静默丢弃。

## 后果

- MPL 源程序可以端到端产出 WAV，并有数值/黄金测试。
- 音频语义独立于 SoundBoxer 的工程/插件生命周期，可稳定回归。
- 代价是参考后端音质有限；它不追求商业级 DSP。
