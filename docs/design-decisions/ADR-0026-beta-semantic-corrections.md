# ADR-0026：Beta 语义修正（bars 锚定与 Rhythm 边界）

状态：已采纳（V0.3.0-rc）

## 背景

Beta 审查发现两处语义不一致：

1. 正整数 `bars` 要求小节锚点，正分数 `bars` 却允许任意锚点；
2. `rotate` 把跨周期持续音切成多段，`rhythmCombine` 对每段都生成 attack，等于改变
   重新触发语义。

## 决策

1. **所有正 bars 锚定在小节边界**：
   - 非小节 anchor 报 `meter-metric-duration`；
   - 正整数逐小节行走，可跨拍号；
   - 正分数按 anchor 处小节长度线性缩放；
   - 负 bars 保持线性位移语义。
2. 明确区分度坐标移动与小节时长：
   - `positionOfBar`/`barIndexAt` 是坐标；
   - `resolveDuration` 是时长；
   - Section 时长还须通过 `isBarStart` 终点检查。
3. Rhythm 边界改用 `continuation`：
   - 首段保留 attack，后续段为 continuation；
   - attackCount/onset 集合/accent/关系算子排除 continuation；
   - combine 默认不 retrigger；
   - `retrigger(rhythm)` 显式重新触发；
   - Tie 未实现，文档明确。
4. MIDI 仍只导出时间签名；grouping 只在 Production/Trace/describe，不伪造元事件。
5. MRP 18.181818 s / 921818 帧复核通过。

## 后果

- `bars` 行为一致且可诊断。
- 跨周期持续音不再静默变成多个 attack。
- 需要 Tie 的作品必须等待后续版本或改用 `retrigger`/周期设计。
