# ADR-0020：Meter Map 与锚定的度量时值

状态：已采纳（V0.3.0-beta）

## 背景

V0.3.0-alpha 只有全局 `Meter`。`bars`/`beats` 在 Sema 阶段折叠为全音符常量，`ProductionIR::meterMap` 只写入一个初始事件。变拍号下这会产生三类错误：

1. 同一 `bars` 在不同 Section 起点应得到不同绝对时值；
2. Section 内的拍号变化无法表达；
3. MIDI 只能导出一个拍号。

## 决策

1. 新增 `MeterMap` 值对象：
   - 有序 `MeterMapEvent`，初始事件固定在位置 0；
   - 每个事件携带 `Meter`、`MetricGrouping` 与绝对 `barIndex`；
   - 拍号变化必须落在小节边界，拍中变化拒绝并诊断。
2. 明确区分：
   - `Position` / `Duration`：绝对全音符有理数；
   - `MetricPosition`：小节 + 小节内拍偏移的坐标表示；
   - `MetricDuration`：N 小节 / N 拍，需 anchor 才能折叠；
   - `Meter`：时间签名；
   - `MetricGrouping`：小节内拍分组。
3. `bars`/`beats` 延迟到 Lowering 解析：
   - 正整数 bars 逐小节边界行走，anchor 必须是小节起点；
   - 分数/负数 bars 按 anchor 处小节长度缩放；
   - beats 使用 anchor 处拍长；
   - 解析失败为 `meter-metric-duration`。
4. Section 拥有局部 `MeterMap`：
   - `section A(4 bars) meter 7/8 grouping 2+2+3 { ... }`；
   - `meter 5/8 at 2 bars grouping 2+3;` 声明局部变化；
   - 未声明时使用 project 初始拍号，不继承前一段落；
   - Section 长度必须落在小节边界。
5. Arrangement 按播放顺序把局部 Map 拼接为全局 Map；`Project::meterMap` 进入 `ProductionIR` 与 Trace。
6. Tempo Map 保持独立，本阶段不做变速。

## 后果

- 变拍号、非均匀分组与 MIDI 拍号事件可精确表达。
- `bars` 的起点依赖是显式、可诊断的，而不是隐藏近似。
- Section 内容可重复播放而不会因播放位置产生不同 IR。
- 拍中变拍号与完整变速系统留给后续版本。
