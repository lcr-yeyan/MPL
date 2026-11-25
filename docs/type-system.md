# MPL V0.1.0 类型系统

## 1. 原则

- 每个表达式在 Typed HIR 中有唯一类型。
- 单位是指定语义的一部分，不进行无依据的隐式换算。
- 数字字面量保持“上下文多态”，但只允许在语义明确的期望类型处转换。
- 时间域（Musical / Relative / Physical / Sample）是独立检查维度。
- 类型检查通过后才构造 Core 对象。

## 2. 类型

| 类型 | 来源 | 说明 |
| --- | --- | --- |
| `Pitch` | 音高字面量 | MIDI 0..127，带拼写 |
| `Interval` | `12 st` | 半音音程，可负 |
| `ScaleDegree` | 整数字面量（上下文） | 音级步数，可负 |
| `Duration` | `1/8`、`2 bars`、`3 beats` | 全音符有理数 |
| `Position` | 时间字面量（上下文） | 片段内偏移 |
| `Tempo` | `160 bpm` | 项目速度 |
| `Frequency` | `200 Hz` | V0.1.0 可解析，无算子消费 |
| `Gain` | `-6 dB` | V0.1.0 可解析，无算子消费 |
| `PhysicalTime` | `100 ms`、`2 s` | 物理时间，保留给音频运行时 |
| `Velocity` | 参数描述 | 1..127 或乘数 |
| `Pan` | 参数描述 | -1..1 |
| `Scalar` | 无单位数字 | 比例、因子 |
| `String` | 字符串 | 资产、路径、片段名 |
| `Fragment` | 音乐字面量/算子 | Music IR 对象 |
| `Rhythm` | `rhythm { ... }` / 节奏算子 | Music IR 节奏值对象（V0.3.0-beta） |
| `Chord` | `chord("Dm7")` / `chordAt` | 和弦符号（V0.3.0-rc） |
| `Harmony` | `harmony { ... }` | 和声时间线（V0.3.0-rc） |
| `Voicing` | `voicing` / `voiceLead` | 具体排列与连接结果（V0.3.0-rc） |
| `Curve` | 曲线声明/算子 | Temporal IR 对象 |
| `List<T>` | `[...]` | harmonize 音级、zip/cycle 音高、accent levels 等 |
| `Asset` | asset 声明 | 符号音源 |
| `Track` | track 声明 | 轨道 |
| `Section` | section 声明 | 段落 |

`Motif` / `Phrase` 在 Core 中都是 `Fragment`；语言层不区分。

V0.3.0-beta 的 `bars`/`beats` 字面量仍是 `Duration` 类型，但携带“度量单位 + 数量”的
常量信息；真正折叠发生在 Lowering 的 MeterMap + anchor 上下文中。`Rhythm` 与
`Fragment` 是不同值类型，不能隐式互转；必须通过 `zip`/`cycle` 组合为 Fragment。

## 3. 字面量与单位

| 写法 | 结果类型 | 说明 |
| --- | --- | --- |
| `160 bpm` | `Tempo` | 项目字段 |
| `4/4` | meter | 仅在 `meter =` 后 |
| `1/8`、`0.5` | `Scalar`（多态） | 在 Duration 位置视为全音符分数 |
| `2 bars` | `Duration` | `2 × barLength` |
| `3 beats` | `Duration` | `3 × beatLength` |
| `1 whole` | `Duration` | 1 全音符 |
| `200 Hz` | `Frequency` | V0.1.0 可解析，无算子消费 |
| `-6 dB` | `Gain` | V0.1.0 可解析，无算子消费 |
| `100 ms`、`2 s` | `PhysicalTime` | 物理时间，保留给音频运行时 |
| `12 st` | `Interval` | 半音 |
| `1/4` in relative curve | `RelativeTime` | [0,1] |
| `"text"` | `String` | 资产 id、路径、clip 名 |
| `F4`、`Ab4`、`C#5` | `Pitch` | 仅限音乐字面量中的音高位置 |

裸数字可以按上下文解释为 `Duration`、`Position`、`ScaleDegree` 或 `Scalar`，但：

- 带单位的值不会退化为裸标量；
- `Interval` 不能用于 `Duration`；
- `Duration` 不能用于 `Interval` / `ScaleDegree`；
- `Tempo` 只能用于 project 的 tempo；
- 所有负数时长在 lowering 前报错。

## 4. 时间域

| 类型 | 表示 |
| --- | --- |
| `MusicalTime` | 全音符有理数 |
| `RelativeTime` | [0,1] 有理数 |
| `PhysicalTime` | 秒（保留） |
| `SampleTime` | 采样序号（保留） |

规则：

- 曲线声明必须指定 `on musical` 或 `on relative`。
- `physical` / `sample` 曲线在 V0.1.0 报 `type-domain-unsupported`。
- 演奏表情（velocity/timing/gate/pan）必须是 Relative 曲线，extent = 1。
- track/master 自动化必须是 Musical 曲线（MIDI CC 导出的当前能力边界）。
- 曲线变换保留输入域。

## 5. 参数与自动化

`ParameterRegistry` 定义：

| 参数 | 单位 | 范围 | 作用域 | 模式 |
| --- | --- | --- | --- | --- |
| volume | ratio | 0..1 | track/master | replace/add/multiply |
| pan | pan | -1..1 | track/master | 同上 |
| expression | ratio | 0..1 | track/master | 同上 |
| modulation | ratio | 0..1 | track/master | 同上 |
| cutoff | ratio | 0..1 | track/master | 同上 |
| resonance | ratio | 0..1 | track/master | 同上 |
| reverb | ratio | 0..1 | track/master | 同上 |
| chorus | ratio | 0..1 | track/master | 同上 |

检查：

- 目标轨道必须存在；
- 参数必须在注册表中；
- 曲线值的关键帧/端点必须在范围内；
- 同目标同域多个 `replace` 报冲突；
- 未知作用域/参数/模式报错。

## 6. 算子签名

```text
transpose(Fragment, Interval) -> Fragment
reverse(Fragment) -> Fragment
stretch(Fragment, Scalar) -> Fragment
repeat(Fragment, Integer) -> Fragment
sequence(Fragment...) -> Fragment
parallel(Fragment...) -> Fragment
diatonicTranspose(Fragment, ScaleDegree) -> Fragment
harmonize(Fragment, List<ScaleDegree>) -> Fragment
syncopate(Fragment, Duration) -> Fragment

constant(Scalar) -> Curve
linear(Scalar, Scalar) -> Curve
step(Scalar, Scalar, Time) -> Curve
smoothstep(Scalar, Scalar) -> Curve
shift(Curve, Time) -> Curve
stretch(Curve, Scalar) -> Curve
reverse(Curve) -> Curve
repeat(Curve, Integer) -> Curve
```

重载 `stretch` / `reverse` / `repeat` 按第一参数类型选择 Fragment 或 Curve 版本。

## 7. 隐式转换

只允许：

- 裸数字字面量 → 期望的 `Duration` / `Position` / `ScaleDegree` / `Scalar`；
- 整数字面量 → `ScaleDegree`；
- 裸数字字面量 → `RelativeTime`（仅相对曲线时间位置）。

不允许：

- `Duration` ↔ `Interval`；
- `Tempo` ↔ `Duration`；
- 不同时间域之间的曲线混用；
- Fragment/Curve 等对象类型的隐式转换。

## 8. 错误码

| 错误码 | 含义 |
| --- | --- |
| `type-mismatch` | 类型不匹配 |
| `type-unit` | 单位/量纲错误 |
| `type-arity` | 参数数量错误 |
| `type-unknown-operator` | 未知算子 |
| `type-domain` | 曲线/自动化时间域错误 |
| `type-domain-unsupported` | physical/sample 曲线不支持 |
| `type-expression-extent` | 表情曲线非归一化 |
| `sema-unknown-name` | 未解析名称 |
| `sema-duplicate-symbol` | 重复声明 |
| `sema-automation-conflict` | 多 Replace 冲突 |
| `sema-parameter-range` | 曲线值越界 |
| `sema-section-overflow` | clip 超出 section（警告，允许重叠） |
| `sema-missing-project` | 缺少 project |
| `sema-missing-arrangement` | 缺少 arrangement |
| `sema-missing-export` | 缺少 export |
| `sema-instance-id-collision` | 稳定实例 id 冲突 |

## 9. V0.2.0-beta：Processor / Bus 类型与参数描述符

类型层新增：

| 类型 | 说明 |
| --- | --- |
| `Processor` | 处理器定义（gain / pan / biquad / compressor / delay / reverb / limiter） |
| `Bus` | 混响/延迟等返回总线；可插入处理器、带 Gain/Pan |

处理器参数不使用 `ParameterRegistry` 的 track 参数表，而使用 `ProcessorParameterDescriptor`：

```text
name / unit / minimum / maximum / defaultValue / text
```

单位与 TypeKind 的对应：

| 描述符单位 | 期望类型 | 示例 |
| --- | --- | --- |
| `Decibel` | `Gain` | `-18 dB`、`4 dB` |
| `Hertz` | `Frequency` | `1200 Hz` |
| `Seconds` / `Milliseconds` | `PhysicalTime` | `10 ms`、`0.12 s` |
| `Ratio` / `Pan` / `Generic` | `Scalar` | `4`、`0.7`、`-0.2` |

校验规则：

- 参数名必须属于该处理器类型；未知参数 → `sema-processor-parameter`。
- 值必须带正确单位；否则 `type-mismatch`。
- 常量值必须落在描述符范围；否则 `sema-parameter-range`。
- `biquad` 必须有合法 mode；其他类型不得带 mode → `sema-processor-mode`。
- Track/Bus/Master 的 `gain` 必须为 `Gain`（dB），`pan` 必须为 `Scalar` 且位于 [-1,1]。
- Send level 必须为 `Scalar` 且位于 [0,1]。
- `insert` 必须引用 `Processor`；`send` 必须引用 `Bus`；`sidechain` 目标必须是已插入当前轨道条的 `Compressor`，源必须是 `Track`。
- 三段自动化的参数使用处理器描述符的范围校验；目标处理器必须已插入目标通道条。`
