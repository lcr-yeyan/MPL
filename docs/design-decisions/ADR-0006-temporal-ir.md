# ADR-0006：Temporal IR 与时间域边界

状态：已采纳（V0.0.3）

## 背景

V0.0.2 只能表达静态音乐结构与片段级来源，无法表达力度渐变、表情、音量自动化等时变属性；同时缺少 Musical / Physical / Relative / Sample 的明确边界。

## 决策

1. 新增独立 `TemporalIR` 值图，节点为 Constant / Linear / Step / Smoothstep / Keyframes / Shift / Stretch / Reverse / Repeat。
2. 曲线求值为有序 `CurveSegment` 列表，可序列化；禁止以任意函数对象作为唯一表示。
3. V0.0.3 的曲线轴只支持 Musical（全音符有理数）与 Relative（[0,1] 有理数）域。
4. 建立 `MusicalTime` / `PhysicalTime` / `RelativeTime` / `SampleTime` 类型边界；Physical / Sample 曲线求值留给音频运行时。
5. 边界规则：段 `[start,end)`，域外保持首/末值；Step 在段内保持 `from`，边界右连续。

## 后果

- 曲线可缓存、失效、序列化与 diff，且不影响 Music IR。
- 表情与自动化可以精确表达在相对/音乐时间域中。
- 限制：没有 tempo map 曲线、采样级曲线或实时调制。
