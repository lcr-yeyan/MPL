# ADR-0015：独立音频运行时与 Backend Contract

状态：已采纳（V0.2.0-beta）

## 背景

V0.2.0-alpha 曾把 SoundBoxer 作为音频执行的默认去向，导致 MPL 的运行时独立性、测试完整性与交付边界不清晰。Beta 起取消该强制依赖；SoundBoxer 只是未来可选后端之一。

## 决策

1. MPL 保持独立的产品、源码、构建、测试与运行时；`mplc build` 直接生成 WAV。
2. 不把 SoundBoxer 的 ProjectModel、AudioEngine、AudioGraph 或其它专有结构引入 MPL Core。
3. 建立 Backend Contract：
   - `BackendCapabilities`：事件精度、处理器自动化、sidechain、Bus/Send、静音终止、最大声道、处理器集合；
   - 音频执行计划；
   - 资产解析（`AssetResolver`）；
   - 参数描述符（`ProcessorParameterDescriptor`）；
   - 渲染配置（采样率、Block、声道、尾音、终止策略）；
   - 输出产物（float32 WAV）；
   - 统一诊断。
4. 不支持语义：
   - `Strict`：错误并拒绝；
   - `Approximate`：警告并近似执行；
   - `Ignore`：警告并忽略该节点。
5. MIDI Backend 与 Reference Audio Backend 保持独立，只共享 Production/Temporal IR。
6. 既有 SoundBoxer 架构审查保留，作为未来 Adapter 的输入；Beta 不实现 Adapter。

## 后果

- MPL 的成功标准不再依赖任何 DAW 产品。
- 后端能力差异可由统一诊断呈现，而不是隐式静音或平台相关失败。
- 未来接入 SoundBoxer 时，需要单独的 Adapter 设计，而不是修改 MPL Core 的数据模型。
