# ADR-0003：Production IR 保持后端无关

状态：已采纳（V0.0.2）

## 背景

V0.0.1 的 `TrackIR` 直接包含 `midiChannel`、`midiProgram`、`percussion`，并在 Production Pass 中分配通道。这使一个通用音乐工程模型携带了 MIDI 导出策略，未来接入 SoundBoxer 或其它 DAW 时会冲突。

## 决策

1. 从 Production IR 删除 MIDI 通道、GM Program、PPQ 与 percussion 标志。
2. 引入后端无关的轨道语义：
   - `InstrumentRole`：melodic / bass / harmony / percussion / texture；
   - `AssetSlot`：命名音源占位、类型、偏好资产；
   - `AssetBinding`：槽位到 assetId/provider 的绑定，允许未解析。
3. MIDI 通道、GM Program 与 percussion 通道策略全部移入 `MidiBackend`。
4. `validate-assets` Pass 只校验槽位/绑定结构，不决定具体音源。

## 后果

- Production IR 可被 MIDI、SoundBoxer 或 DAW 工程后端共同消费。
- MIDI 导出的通道分配是确定性策略，但不属于音乐语义。
- 音色选择可以留给人工或未来后端；未解析绑定是合法状态。
