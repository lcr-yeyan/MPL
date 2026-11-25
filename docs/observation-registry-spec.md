# Observation Registry Specification（V1）

状态：V0.4.0-beta 实现规范

代码：`include/mpl/observe/Observations.hpp`、`src/observe/Observations.cpp`

## 1. 定位

Observation Registry 把“音乐程序的可验证事实”从源语言语法中独立出来。每个观察有：

- 唯一名称；
- 输入类型；
- 作用域；
- 单位；
- 计算语义；
- 适用条件；
- 比较类别：`exact` / `tolerance` / `unverifiable`。

观察值不是艺术评价，也不声称作品“好听”。验证器只比较声明的事实。

## 2. 值模型

```cpp
struct ObservationValue {
    std::string name;
    std::string status;    // exact / tolerance / unverifiable
    bool available;
    std::string jsonValue; // canonical JSON
    std::string reason;
};
```

- 精确观察必须逐结构相等；
- 容差观察必须为数值，按显式容差比较；
- 不可验证观察返回 `unverifiable`，不返回 pass/fail。

`ObservationSet::toJson()` 输出稳定对象：

```json
{
  "format": "mpl.observations",
  "version": 1,
  "observations": [
    {"name":"event_count","status":"exact","available":true,"reason":"","value":"247"}
  ]
}
```

`value` 是规范化 JSON 字符串；这样浮点、分数与嵌套结构的表示不依赖宿主语言。

## 3. 注册表

| 名称 | 输入 | 作用域 | 单位 | 类别 | 语义 |
| --- | --- | --- | --- | --- | --- |
| `logical_extent` | project | project | whole-notes | exact | `ProductionIR.logicalExtent`，含有意静默 |
| `content_extent` | project | project | whole-notes | exact | 所有 clip 音符绝对结束的最大值，不含尾部逻辑留白 |
| `event_count` | project | project | count | exact | `ProductionIR.noteCount()` |
| `onset_set` | track/clip | project | whole-notes | exact | 排序去重的绝对 note onset |
| `onset_density` | project | project | events/whole-note | tolerance | `event_count / logical_extent` |
| `pitch_sequence` | track | track | midi | exact | 每轨按绝对 onset、MIDI 排序的 pitch |
| `interval_sequence` | track | track | semitones | exact | 每轨相邻 pitch 的 MIDI 差 |
| `normalized_rhythm` | track | track | ratio | exact | 每轨 IOI 除以逻辑长度，精确有理数 |
| `harmonic_timeline` | Harmony | project | events | exact | 所有已求值 harmony-literal 的逻辑长度与事件 |
| `fragment_range` | clip | clip | semitones | exact | 每个 clip 的最高-最低 MIDI |
| `voice_movement` | Voicing sequence | voice | semitones | unverifiable | 需要显式 voicing 序列，未从工程自动推导 |
| `module_dependencies` | module | project | hash | exact | 模块 contentHash、resolvedHash、imports、exports |
| `track_symbolic_events` | track | track | event | exact | 每轨绝对 onset/duration/MIDI/velocity |
| `sample_peak` | audio | audio | dBFS | tolerance | 4x 过采样前的 sample peak |
| `true_peak` | audio | audio | dBTP | tolerance | 4x 过采样 true peak |
| `integrated_loudness` | audio | audio | LUFS | tolerance | BS.1770 风格 gated integrated loudness |

## 4. 适用条件

- `onset_density`、`normalized_rhythm`：逻辑长度为 0 时不可验证。
- `voice_movement`：从 `Compilation` 不可推导；V0 明确返回 `unverifiable`，不猜测。
- 音频三项：未提供 WAV 或 WAV 不可读时不可验证；静音/无 gating block 时 integrated loudness 不可验证。
- `harmonic_timeline`：没有 harmony 值时为空数组，而不是错误。
- `module_dependencies`：无模块时为空数组。

## 5. 确定性

- onset、track、clip 顺序使用稳定排序；
- 有理数使用规范字符串；
- 浮点使用宿主 JSON 双精度输出，容差比较；
- 相同 `Compilation` 与相同输入产生逐字节相同的 `toJson()`（测试 `ObservationSetIsDeterministic`）。

## 6. 与任务验证的关系

任务 JSON 通过名称引用观察。目标观察必须有显式值或 `fromBaseline`；保持观察以 baseline
值为期望。验证器不把模型/生成器的声明当作事实。

## 8. V1 元数据与语义澄清

每项观察现在包含：

| 字段 | 值 |
| --- | --- |
| `id` | 稳定 ID，等于名称 |
| `version` | 1 |
| `inputType` | project / track / clip / Harmony / module / audio |
| `outputType` | rational / integer / number / array |
| `unit` | whole-notes / midi / count / ratio / dBFS / dBTP / LUFS |
| `scope` | project / track / clip / module / audio |
| `preconditions` | 与 `conditions` 相同，机器可读 |
| `comparisonPolicy` | exact / tolerance / unverifiable |
| `verificationStatus` | `verified`（注册表项） |

V1 语义：

- `voice_movement`：新增显式 API `observeVoiceMovement(voicings)`；声部对应按列表索引，
  每个 voicing 必须声部数相同；总移动为相邻 voicing 同索引声部绝对移动之和。
  另设独立参考验证器 `verifyVoiceMovementReference`，不调用编译器的 `voiceLead`。
- `normalized_rhythm`：每轨相邻绝对 onset 差除以 `logical_extent`，精确有理数；
  逻辑长度为 0 时不可验证。
- `onset_density`：`event_count / logical_extent`，单位 events/whole-note，作用域 project。
- `harmonic_timeline`：规范比较为“逻辑长度 + 事件 start/duration/chord 序列”；事件按创建
  顺序，不按 ObjectId。
- `track_symbolic_events`：每个 track 内按绝对 onset、MIDI、duration 稳定排序；重复音符
  保留，比较是多重集/序列规范，不合并同音。
- 音频观察必须绑定可信渲染产物：`ObservationOptions.expectedAudioHash` 不匹配时
  sample/true peak 与 integrated loudness 均为 `unverifiable`，不读取为当前候选结果。

## 9. 参考验证器

小规模声部连接任务可使用独立参考验证器：

```cpp
VoiceMovementReference reference = verifyVoiceMovementReference(voicings);
```

它只检查声部数一致并按索引求和，不依赖 `voiceLead` 的搜索、预算或状态。用于确认
`voice_movement` 观察本身，而不是替代完整声部连接验证。

## 10. 范围限制

V1 只覆盖上述 16 项；不包含和声功能、风格、情绪、音频频谱或主观质量。新增观察需要：

1. 在注册表声明 id/version/input/output/unit/scope/preconditions/policy/status；
2. 实现稳定计算；
3. 增加精确/容差/不可验证分类；
4. 增加回归测试。

