# ADR-0032：节奏、和声与音高的独立替换

状态：已采纳（V0.3.0-rc）

## 背景

创作中常需要“节奏不变换和声”或“和声不变换节奏”。如果把它们合并成一个生成器，
就无法独立验证每个维度，也无法保留生成依赖。

## 决策

1. 三个维度保持独立值：
   - Rhythm：起音/时值/重音/休止；
   - HarmonyTimeline：和弦事件；
   - Pitch strategy：voicing/voiceLead/zip/cycle。
2. 组合通过显式函数：
   - `chordAt`/`chordAtBar` 选择和声；
   - `voicing`/`voiceLead` 生成音高；
   - `zip`/`cycle` 组合节奏与音高；
   - 用户函数按小节组织。
3. 不做隐式“生成器”或“风格模板”。
4. 替换测试：
   - 同 Rhythm、不同 Harmony → 起音数相同、音高不同；
   - 同 Harmony、不同 Rhythm → 起音数不同；
   - 和声时间线可被多个声部共享。
5. 生成依赖：
   - Harmony 节点/查询进入 Trace；
   - Rhythm 事件与 note 的 rhythmOrigin/rhythmEventIndex 保留；
   - 音高组合节点记录输入；
   - 每个 Clip 仍是独立 Performance Instance。

## 后果

- 可以独立修改节奏或和声并验证影响范围。
- 不引入通用约束求解器。
- 生成关系可追溯，但跨编译缓存仍未实现。
