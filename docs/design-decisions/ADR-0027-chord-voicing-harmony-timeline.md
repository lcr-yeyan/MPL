# ADR-0027：和弦、排列与和声时间线

状态：已采纳（V0.3.0-rc）

## 背景

Alpha/Beta 只有 Pitch、Interval、Key 与 harmonize 算子。缺少 PitchClass、ChordSymbol、
Voicing、PitchClassSet 与可查询的和声时间线，无法表达和弦转位、扩展音、共享和声与
和声驱动声部。

## 决策

1. 新增 `PitchClass`、`PitchClassSet`、`ChordSymbol`、`Voicing`、`HarmonyTimeline`。
2. `ChordSymbol` 保存显式 interval set，而不是只保存质量枚举：
   - 支持三和弦、6/7/9/11/13、变化音、slash bass；
   - 转位由 bassPitchClass 表达；
   - `pitchClassSet()` 为唯一和弦音查询。
3. `HarmonyTimeline` 是独立值：
   - 事件 `[start, start+duration)`；
   - 允许重叠/空白；`eventAt` 取 start 最大者，同 start 后者优先；
   - 边界右连续，gap 返回空和弦；
   - 逻辑长度可显式延长。
4. Music IR 增加 `harmony-literal` 与 `harmony-query`，参与依赖/缓存/Trace。
5. 和声不直接生成音符；必须与 Rhythm + 明确音高策略组合。
6. 不实现完整微分音/调律；MIDI 0..127 只是编码边界。

## 后果

- 同一和声时间线可被多个声部共享。
- 和弦身份、排列与音高事件分离，便于替换和验证。
- 功能分析和风格化和弦选择不在默认范围。
