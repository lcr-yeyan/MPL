# ADR-0008：自动化绑定、组合模式与后端能力诊断

状态：已采纳（V0.0.3）

## 背景

自动化既需要后端无关的参数目标，又必须避免 MIDI CC 编号、通道、PPQ 污染通用 IR。同时后端无法表达的自动化不能被静默丢弃。

## 决策

1. 通用 IR 使用 `ParameterTarget`（scope / parameter / unit / range）与 `AutomationBinding`（curve / domain / mode / enabled）。
2. 组合模式：
   - Replace 提供基值；
   - Add 求和；
   - Multiply 求积；
   - 缺失 Replace 时，Add 基值为 0，Multiply 基值为 1。
3. 同一参数、同一时间域多个 Replace 是 `automation-conflict` 错误；Add/Multiply 可组合。
4. 曲线值必须落在参数范围内，否则编译诊断；组合结果越界时后端夹紧并产生诊断。
5. MIDI 后端自行完成 CC 编号、通道、量化与采样；通用 IR 不含这些字段。
6. 无法表达的情况（master/clip 作用域、relative 域、未知参数、逐音符 pan、tick 量化）必须产生显式诊断。

## 后果

- 自动化语义与后端解耦，可被 SoundBoxer / DAW 后端复用。
- 组合与冲突行为可测试、可解释。
- 限制：MIDI CC 导出是 1/16 全音符采样；实时调制与感知策略不在本阶段。
