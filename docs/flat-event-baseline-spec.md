# Flat Event Baseline Specification（V0）

状态：V0.4.0-beta

代码：`include/mpl/lab/FlatEvents.hpp`、`src/lab/FlatEvents.cpp`

## 1. 定位

Flat Event JSON 是独立于 MPL 语法的直接事件表示，用作实验对照。它只描述已生成事件，
不携带函数、模块、和声生成来源或声部搜索过程。

## 2. Schema

```json
{
  "format": "mpl.flat-events",
  "version": 1,
  "logicalExtent": "2",
  "tempoBpm": 120,
  "meterMap": [
    {"position": "0", "numerator": 7, "denominator": 8, "grouping": "2+2+3"}
  ],
  "tracks": [
    {
      "id": "Piano",
      "events": [
        {"pitch": 60, "onset": "0", "duration": "1/4", "velocity": 96},
        {"pitch": "E4", "onset": "1/4", "duration": "1/4", "velocity": 90}
      ]
    }
  ],
  "taskContext": "optional external task id"
}
```

字段：

| 字段 | 必需 | 语义 |
| --- | --- | --- |
| `logicalExtent` | 是 | 精确有理数 whole notes，含静默 |
| `tempoBpm` | 是 | 正数，初始 tempo |
| `meterMap` | 是 | 至少一项，position 0 开始；拍号与 grouping |
| `tracks[].id` | 是 | 唯一 Track ID |
| `events[].pitch` | 是 | MIDI 0..127 或音名 |
| `events[].onset` | 是 | 精确有理数 whole notes |
| `events[].duration` | 是 | 正有理数 whole notes |
| `events[].velocity` | 是 | 1..127 |
| `taskContext` | 否 | 任务/资产上下文标签 |

**禁止浮点拍数。** `onset: 0.25` 会被拒绝；必须写 `"1/4"`。

## 3. 共同可观察表示

Flat 与 MPL 共用 Observation Registry 与 Task Verifier。支持：

- `logical_extent`
- `content_extent`
- `event_count`
- `onset_set`
- `onset_density`
- `pitch_sequence`
- `interval_sequence`
- `normalized_rhythm`
- `track_symbolic_events`
- `sample_peak` / `true_peak` / `integrated_loudness`（需可信 WAV）

不支持：

| 观察 | 原因 |
| --- | --- |
| `harmonic_timeline` | flat 事件没有和声时间线 |
| `module_dependencies` | flat 事件没有模块来源 |
| `voice_movement` | flat 事件没有显式声部对应 |
| `fragment_range` | flat 只有 track，不是 MPL clip/source 范围 |

对这些观察的 target/preserve，验证器返回 `not-applicable`，不返回 pass/unverifiable。

## 4. 能力覆盖率

任务在 Flat 下的适用性：

- 01 basic：适用；
- 02 asymmetric meter：适用（事件可表达拍号/onset）；
- 03 multi-period：适用（事件可表达起音集合）；
- 04 theme transform：适用（只要目标是音高序列）；
- 05 harmony-driven：不适用（目标含 harmonic_timeline）；
- 06 chord voicing：不适用（目标含 fragment_range）；
- 07 voice leading：适用（如果只比较音高序列/事件）；
- 08 rhythm replacement：不适用（preserve 含 harmonic_timeline）；
- 09 harmony replacement：适用（比较音高/节奏，不需来源）；
- 10 local edit：不适用（任务声明 frame，Flat 无生产级 frame）；
- 11 mix automation：不适用（任务声明 frame；Flat 无生产级 frame）；
- 12 mastering：不适用（Harness 未为 Flat 候选绑定可信音频）。

覆盖率由 `flatSupportsObservation` 决定；不得把不适用任务混入共同成功率。

## 5. 验证

```text
mpl_lab verify bench/tasks/01-basic-notes.json candidate-flat.json
```

- 基线始终是 MPL 程序；Flat 候选直接转换为同一观察集；
- target/preserve 使用相同比较规则；
- 任务 `frame` 存在时，Flat 无法做生产级 frame 验证 → `not-applicable`；
- 音频观察可使用 `--audio`，但必须通过可信 manifest 的 hash 绑定（见 Trusted Render）。

## 6. 限制

- 无生成来源/函数调用信息；
- 无和声、模块、声部搜索；
- 不要求 Flat 提供其表示本身没有的信息；
- 不把 Flat 事件视为 MPL 源程序的等价替代。
