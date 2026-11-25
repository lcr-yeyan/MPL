# ADR-0021：Rhythm 作为 Music IR 值节点

状态：已采纳（V0.3.0-beta）

## 背景

Alpha 没有独立 Rhythm 类型：节奏只能是 Fragment 的 offset/duration 集合，无法脱离音高参数化，也无法直接表达休止、逻辑长度、周期与相位。

备选方案：

1. 独立的 `RhythmIR` 图；
2. 把 Rhythm 复制进 Fragment；
3. 在 Music IR 中增加 Rhythm 值节点，与 Fragment 共用 `ObjectId` 空间。

## 决策

采用方案 3。

1. `Rhythm` 是独立值类型，包含逻辑长度与事件（onset、duration、accent、rest、origin）。
2. Music IR 增加 Rhythm 节点族：
   - literal / sequence / overlay / repeat / rotate / shift / stretch / subdivide / accent / realign / quantize / accent-by-meter / intersect / union / difference / complement / accent-follow；
   - `rhythm-combine` 产出 Fragment。
3. `MusicNode` 同时可缓存 `Fragment` 与 `Rhythm`；`evaluate` 与 `evaluateRhythm` 分开，类型错误显式抛出。
4. Rhythm 与 Fragment 共用依赖图、ObjectId、失效与 Trace 机制；同一 Rhythm 可被多个组合节点共享。
5. `Note` 增加 `rhythmOrigin` 与 `rhythmEventIndex`，逐音追溯到 Rhythm 事件。

## 理由

- 不引入第二套图、id 空间与缓存；
- Rhythm 变换天然参与 `dependents`/`invalidate`；
- Trace 可以同时展示节奏与音高的生成链；
- 不需要把节奏复制成带 dummy pitch 的 Fragment。

## 后果

- `MusicIR::evaluate` 对 Rhythm 节点会报“不是 Fragment”，避免隐式转换。
- 外部代码若假设所有节点都是 Fragment，需要改用 `isRhythmObject`/`evaluateRhythm`。
- Rhythm 与音高组合仍需要一个显式 pitch sequence；不存在隐式音高分配。
