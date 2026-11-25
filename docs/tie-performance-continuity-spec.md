# Tie and Performance Continuity（V0.5.0-beta）

## 1. 语义

```text
tie(first: Fragment, second: Fragment) -> Fragment
```

显式连接两个片段边界上的**一个**逻辑音：
- `first` 最后一个在 `first.duration` 结束的音；
- `second` 在 0 开始的音；
- 两者 MIDI 音高相同；
- 合并为一个 Note，duration = 两段 duration 之和；
- span = first.duration + second.duration；
- `second` 的其余音符按 first.duration 平移。

`tie` **不是**“所有相邻同音高自动合并”。未显式 `tie` 的同音高相邻音仍是两个 attack。

## 2. 边界条件

| 条件 | 结果 |
| --- | --- |
| 任一片段为空 | `tie-empty` |
| 边界音高不同 | `tie-pitch` |
| 一侧边界有多个音（和弦/多声部） | `tie-ambiguous` |
| 边界音不是 last/first | `tie-ambiguous` |
| 普通相邻同音高未 tie | 两个 Note，两个 MIDI note-on |

## 3. 逻辑事件 vs 演奏事件

- 逻辑事件：一个 `Note` 的 duration 跨边界；
- 演奏事件：MIDI 只产生一个 note-on/note-off；WAV 只产生一个 gate/envelope；
- 不产生 orphan continuation：边界不合法直接诊断，不写入半连接事件；
- 不引入隐藏的 `continuation` 音符。

## 4. 声部身份与来源

- 当前 `Fragment::Note` 没有 voice id；为保证正确性，边界存在多个音时**拒绝** tie，而不是猜测声部。
- 合并音保留 first 边界音的 `origin`；second 的 origin 不单独保留（已知限制）。
- ObjectId：tie 是一个新的 Music IR 节点，输入为两个片段。

## 5. MIDI / 音频 / 自动化

- MIDI：合并后一个 note-on，一个 note-off；测试统计 note-on 数为 1。
- 原生音频：一个 note 事件，gate 持续整个合并 duration；不做 retrigger。
- 自动化：tie 不改变曲线/自动化时间轴；持续音跨越自动化事件时仍按原时间轴执行。
- 跨 clip/section 的 tie 不自动成立；必须在同一 Fragment 内显式 tie。

## 6. 测试

`tests/test_ties.cpp`：

- 跨边界合并为一个 logical note，event_count=1，MIDI note-on=1；
- 未 tie 的相邻同音高 event_count=2，note-on=2；
- pitch mismatch / ambiguous / empty 诊断；
- 跨拍号 section 的 span 保持 7/4。

## 7. 已知限制

- 不支持链式 `tie(a,b,c)` 的专用语法（可嵌套调用）；
- 不支持按 voice id 选择声部；
- 不保留 second 边界音的独立 origin；
- 不允许 orphan/部分连接；
- 跨 clip 的 tie 尚未支持；
- 没有专门的 performance-event 层，Tie 目前在逻辑 Note 上直接实现。
