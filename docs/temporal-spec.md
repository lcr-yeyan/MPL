# MPL 时序语义规范（Temporal IR v1，Core v0.0.3）

## 1. 目标与范围

Temporal IR 用**可序列化的值图**表达音乐属性的时变性：力度、表情、音量、调制等。它不是音频运行时，也不实现实时调制、DSP 或采样级曲线求值。

设计约束：

1. 曲线必须是可序列化的节点图，不能只依赖任意函数对象。
2. 音乐时间保持有理数，BPM / 采样率不进入曲线结构语义。
3. 曲线修改只失效 Temporal IR 自身，不触发 Music IR 重算。
4. 后端无法表达的语义必须产生诊断。

## 2. 时间域

| 类型 | 表示 | 单位 | V0.0.3 曲线 |
| --- | --- | --- | --- |
| `MusicalTime` | `Position`（有理数） | 全音符 | 支持 |
| `RelativeTime` | `Rational` ∈ [0,1] | 实例内归一化位置 | 支持 |
| `PhysicalTime` | `double` 秒 | 秒 | 保留类型边界 |
| `SampleTime` | `int64` | 采样序号 | 保留类型边界 |

- Physical / Sample 曲线在 V0.0.3 创建时抛 `temporal-domain`。
- `sampleAt` 会检查曲线域与传入时间类型是否一致。
- BPM 只用于 `Duration::toSeconds`、MIDI 导出和音频运行时；曲线本身不含 BPM。

## 3. 曲线表示

```cpp
struct CurveSegment {
    Rational start;
    Rational end;
    double from;
    double to;
    Interpolation interpolation; // Step | Linear | Smoothstep
};

struct Curve {
    TimeDomain domain;
    Rational extent;
    std::vector<CurveSegment> segments;
};
```

约定：

- 段按 `start` 升序、互不重叠。
- 段区间为 `[start, end)`；正好落在边界时取后一段。
- 定义域外保持首段 `from` 或末段 `to`。
- Step：在 `[start, end)` 内保持 `from`，到 `end` 时由后续段切换。
- Linear：`from + (to-from) * x`。
- Smoothstep：`from + (to-from) * (3x² - 2x³)`。
- 曲线值必须有限（`std::isfinite`）。

## 4. 节点语义

| 节点 | 参数 | extent | 说明 |
| --- | --- | --- | --- |
| `constant` | value | 指定 | 单段常值 |
| `linear` | from, to | 指定 | 单段线性 |
| `step` | from, to, switchAt | 指定 | switchAt 必须在 [0, extent] |
| `smoothstep` | from, to | 指定 | 单段平滑插值 |
| `keyframes` | points | 指定 | 时间严格递增、位于 [0, extent] |
| `shift` | amount | 与输入相同 | 段整体平移，域外按边界保持 |
| `stretch` | factor > 0 | 输入 × factor | 时间刻度与 extent 同步缩放 |
| `reverse` | 无 | 与输入相同 | 时间镜像；Step 边界按右连续约定处理 |
| `repeat` | count ≥ 1 | 输入 × count | 平铺；边界取下一份的起始值 |

Keyframes 的 `interpolation` 描述从该关键帧到下一关键帧的段。首帧之前的区间保持首值，末帧之后的区间保持末值。

## 5. 校验与错误

| 情况 | 错误码 |
| --- | --- |
| 非正 extent | `temporal` |
| 非有限值 | `temporal` |
| 未知输入 id | `temporal`（操作名） |
| Keyframe 时间越界/非严格递增 | `temporal` |
| Step switchAt 越界 | `temporal` |
| 非正 stretch 因子 / repeat 次数 < 1 | `temporal` |
| Physical / Sample 域曲线 | `temporal-domain` |
| 采样时间类型与曲线域不一致 | `temporal-domain` |

## 6. 缓存、失效与序列化

- 节点按 id 升序构成 DAG，`evaluateAll` 即拓扑求值。
- `evaluate` 缓存 `Curve`；命中计数用于验证增量行为。
- `updateConstant` / `updateLinear` / `updateKeyframes` 修改参数并失效该节点及下游。
- `toJson` 输出节点 id、kind、label、domain、extent、inputs、参数与已求值段。
- 不序列化“求值过的函数”，只序列化参数与结果段，因此可重建、可 diff。

## 7. 演奏表情

`PerformanceExpression` 属于 clip 播放实例：

| 通道 | 含义 | 范围 |
| --- | --- | --- |
| velocity | 力度乘数 | 0.05..2.0 |
| timing | 起始偏移（全音符） | -1/2..1/2 |
| gate | 时值乘数 | 0.1..2.0 |
| pan | 声像 | -1..1 |

- 必须是 Relative 域、extent = 1。
- 采样点：`note.offset / clip.duration`。
- 时值变化会自然改变相对位置，因此表情自动重新映射。
- MIDI 后端支持 velocity / timing / gate；逐音符 pan 产生 `midi-unsupported-expression` 并忽略。
- timing 不能精确落在 tick 时产生 `midi-quantization`。

## 8. 自动化绑定

```text
ParameterTarget
├── scope: track | clip | master
├── parameter: 稳定名称
├── unit: generic | decibel | ratio | pan | milliseconds
└── range: minimum..maximum
```

```text
AutomationBinding
├── target
├── curve
├── domain: musical | relative
├── mode: replace | add | multiply
└── enabled
```

组合：

```text
base = replace ? replaceValue : (hasAdd ? 0 : 1)
combined = (base + Σ add) * Π multiply
```

- 同一参数、同一域出现多个 `replace` → `automation-conflict`。
- 每条曲线在关键帧/段端点上的值必须落在目标范围内；越界 → `automation-range`。
- 组合结果越界时后端夹紧并产生 `midi-automation-range`。
- 目标不存在 → `unknown-target`；曲线不存在 → `unknown-curve`；域不匹配 → `automation-domain`。

## 9. MIDI 映射

| 参数名 | CC |
| --- | --- |
| `modulation` | 1 |
| `volume` | 7 |
| `pan` | 10 |
| `expression` | 11 |
| `resonance` | 71 |
| `cutoff` | 74 |
| `reverb` | 91 |
| `chorus` | 93 |
| `ccN` | N（0..127） |

- 只导出 track 作用域、Musical 域的绑定。
- 采样步长 1/16 全音符，最多 2048 个采样点；包含曲线终点。
- 单位映射：Ratio/Generic 使用目标范围，Pan 从 [-1,1] 映射，Decibel 使用目标范围。
- master/clip 作用域、relative 域、未知参数、逐音符 pan 都产生明确诊断。

## 10. 身份稳定性

- 定义对象：`ObjectId`，创建顺序稳定。
- 播放实例：`InstanceId`，按 lowering 顺序分配。
- 音符实例：`NoteInstanceId = (InstanceId, noteIndex)`，trace 输出 `"instance:index"`。
- 参数：`ParameterId` 由名称构成（如 `track:Piano:volume`），不随轨道重排改变。

## 11. 已知限制

1. Physical / Sample 域曲线尚未求值。
2. 没有实时调制、生成式自动化或感知策略。
3. 没有 tempo map 曲线；Tempo 仍为单一事件。
4. 表情是 clip 级共享曲线，不是逐音符独立自动化。
5. MIDI CC 采样是离散近似，不保证采样级还原。
6. `shift` 超出 extent 的部分被边界保持规则截断。
