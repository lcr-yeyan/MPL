# MPL 语言与核心语义规范（V0.3.0 Final）

状态：V0.3.0 正式版。语义审计通过，语言、编译器与运行时冻结。本文描述 Music/Temporal/Production IR、前端语义与音频执行契约；V0.3.0 新增 MeterMap、独立 Rhythm、音高/和声/声部关系与最小模块系统。

## 1. 定位

MPL 是音乐生产程序设计语言，而非 MIDI DSL。编译产物应包含：

1. 可编辑音乐工程（Production IR，未来映射到 SoundBoxer 工程）。
2. 音乐事件/工程文件（V0.0.2 为 MIDI）。
3. 完整音频作品（未来由 SoundBoxer 音频后端渲染）。

## 2. 程序模型

一次编译由 `Project` 描述：

```text
Project
├── tempo, meter, key
├── MusicIR        音乐语义依赖图
└── TrackSpec[]    轨道 + 多个 ClipSpec
    ├── name / instrument / InstrumentRole
    └── ClipSpec[] (name, part: ObjectId, start: Position)
```

`TrackSpec` 只引用 Music IR 对象，不复制音符；同一对象可被多个轨道或多个段落复用。

## 3. 时间语义

- Music IR 与 Production IR 的时间单位是**全音符的有理数**：`1` = 全音符，`1/4` = 四分音符，`1/8` = 八分音符。
- 4/4 的一小节 = `1`；3/4 的一小节 = `3/4`；6/8 的一小节 = `3/4`；7/8 的一小节 = `7/8`。
- `Duration` 与 `Position` 都基于同一有理数时间，**不存在浮点时间**。
- Tempo 只用于秒数换算和 MIDI 导出：全音符时长 = `240 / bpm` 秒。
- PPQ 只属于 MIDI 后端，不进入 Music IR 或 Production IR。

校验例：160 BPM、4/4、4 小节。

```text
4 小节 × 1 全音符/小节 = 4 全音符
4 × 240 / 160 = 6 秒
4 小节 × 4 拍 × 480 tick/拍 = 7680 tick（PPQ=480）
```

V0.0.1 示例曾把 5 个音写成 `1/2+1/2+1+1+1 = 4` 全音符，却标注为“1 小节”，因此得到 16 小节、24 秒。换算本身正确，是示例时值与文字口径错误。V0.0.2 已把该动机改为 `1/8+1/8+1/4+1/4+1/4 = 1` 全音符，并有端到端测试锁定 6 秒与 7680 tick。

## 4. 类型

| 类型 | 语义 |
| --- | --- |
| `Rational` | 精确有理数，所有音乐时间的基础 |
| `Meter` | 拍号；派生小节长度与拍长度 |
| `Duration` | 音乐时值（全音符为单位） |
| `Position` | 音乐时间位置（全音符为单位） |
| `Pitch` | 带拼写的音高（字母、变音记号、八度、MIDI 编号） |
| `Interval` | 半音音程 |
| `Note` | 音高 + 相对偏移 + 时值 + 力度 + 来源 `origin` |
| `Fragment` | 带显式 span 的音乐片段 |
| `Motif` / `Phrase` | `Fragment` 的角色别名 |
| `Key` | 主音拼写 + `major` / `natural minor` |
| `InstrumentRole` | melodic / bass / harmony / percussion / texture |
| `AssetSlot` | 轨道内命名的音源占位 |
| `AssetBinding` | 槽位到具体音源/采样与 provider 的绑定 |

`Note::origin` 是编译期元数据，不属于音乐内容；所有确定性变换原样复制它，`harmonize` 生成的音以该算子对象 id 为来源。

## 5. 算子

### 5.1 确定性算子（无上下文依赖）

- `transpose(fragment, interval)`：按半音移调。
- `reverse(fragment)`：在片段 span 内做时间镜像。
- `stretch(fragment, factor)`：缩放偏移、时值与 span，factor 必须为正。
- `repeat(fragment, count)`：按时间顺序复制。
- `sequence(parts...)`：顺序拼接。
- `parallel(parts...)`：同起点叠加。

### 5.2 上下文相关算子（V0.0.2 实验）

- `diatonicTranspose(fragment, scaleSteps)`：读取当前 `Key`，按音级移动，保留输入音级的半音变化。
- `harmonize(fragment, degreeOffsets)`：读取当前 `Key`，为每个音叠加指定音级偏移的和弦音，生成音力度为 80。
- `syncopate(fragment, displacement)`：读取当前 `Meter`，把所有落在拍点上的音位移 `displacement`。

三者的语义是“接口与可追溯性优先”，不代表作曲质量。它们不包含搜索、生成模型或约束求解。

## 6. 上下文

`MusicIR` 持有：

- `Key`：默认 C major；
- `Meter`：默认 4/4。

算子节点声明上下文依赖（key / meter）。修改上下文时，只失效声明了对应依赖的节点及其下游：

- `setKey` 只影响 `diatonicTranspose`、`harmonize` 及其依赖者；
- `setMeter` 只影响 `syncopate` 及其依赖者。

上下文不是隐式全局可变状态：它保存在 IR 中，并写入 trace，便于复现。

## 7. 编译管线

```text
Project
  → MusicIR.evaluateAll()        依赖顺序求值并缓存
  → lower to ProductionIR        展开 track/clip/绝对时间线，绑定资产
  → PassManager                   validate-production, validate-assets
  → MidiBackend                   MIDI 专属的通道/音色/PPQ/量化
```

- Music IR → Production IR 时保留 `sourceObject` 与 `sourceLabel`。
- 轨道不再携带 MIDI 通道、GM 音色或 PPQ。
- 资产绑定允许未解析（空 `assetId` 或 `provider`），表示等待人工或 SoundBoxer 选择。

## 8. 诊断与确定性

- 图构建期错误抛 `mpl::Error`（未知对象、非法参数、越界音高、溢出）。
- 生产期问题进入 `Diagnostic`（错误/警告），一次编译可报告多条。
- 序列化 JSON 的字段顺序固定；黄金测试锁定 Music IR、Production IR 与 MIDI 字节。
- 随机性尚未进入 Core；生成性算子未来必须显式携带 seed 与模型版本。

## 9. 表达力边界（已测试）

支持：

- 跨小节音符与尾部休止（显式 span）；
- 三连音（1/12 全音符在 PPQ=480 下为 160 tick，无量化误差）；
- 复合拍号（6/8、7/8）的精确小节位置；
- 多声部：`parallel` 叠加或多轨道；
- 多段落：同一轨道多个命名 clip。

尚不支持：

- 声部/谱表级语义、连线、力度曲线、演奏法；
- 调式交替、和弦符号、和声进行约束；
- 自动配器、混音与母带节点；
- 文本语法、自然语言入口。

## 10. Phase 1 映射建议

Phase 1 文本前端应把语法糖编译到本规范的对象：

```text
project { tempo = 160 bpm; meter = 4/4; key = F minor; }
asset Piano = instrument("GrandPiano");

motif A = notes { F4: 1/8, Ab4: 1/8, C5: 1/4 };
let B = diatonic_transpose(A, +2);

section Intro(4 bars) {
    track Piano role melodic { play A; }
}
```

语法层可选“拍”为单位的人类写法，但必须在 AST→Music IR 降低时显式换算为全音符有理数，并保留单位类型检查。

## 11. V0.0.3：时变属性、实例与自动化

### 11.1 时间域

| 类型 | 单位 | 用途 | V0.0.3 曲线轴 |
| --- | --- | --- | --- |
| `MusicalTime` | 全音符有理数 | 结构、轨道自动化 | 支持 |
| `RelativeTime` | [0,1] 有理数 | 演奏表情、实例内归一化 | 支持 |
| `PhysicalTime` | 秒（double） | 音频运行时、导出换算 | 保留 |
| `SampleTime` | 采样序号（int64） | DSP 运行时 | 保留 |

曲线只使用 Musical / Relative 两个域；Physical / Sample 域的类型边界已经建立，但曲线求值留给音频运行时。BPM 与采样率不进入 Temporal IR 的结构语义。

### 11.2 曲线

`TemporalIR` 是独立的、可序列化的有向无环曲线图，节点类型：

- `constant(value)`
- `linear(from, to)`
- `step(from, to, switchAt)`
- `smoothstep(from, to)`
- `keyframes(points)`，每个关键帧指定到下一帧的插值
- `shift(curve, amount)`、`stretch(curve, factor)`、`reverse(curve)`、`repeat(curve, count)`

所有节点都有 `domain` 与 `extent`（定义域长度）。求值结果是分段曲线，段左闭右开 `[start, end)`；域外保持首/末值。插值规则：

```text
step:       value = from, t ∈ [start, end)
linear:     value = from + (to - from) * x
smoothstep: value = from + (to - from) * (3x² - 2x³), x = (t-start)/(end-start)
```

`shift` 保持 extent 不变，域外由边界保持规则处理；`stretch` 将 extent 与时间刻度乘以正因子；`reverse` 在 extent 内镜像；`repeat` 平铺 `count` 次并把 extent 乘以 count。

### 11.3 Definition / Instance

- `ObjectId`：Music IR 定义对象 id，创建顺序稳定。
- `InstanceId`：Production IR 中一次播放实例的 id，按轨道/片段顺序确定性分配。
- `NoteInstanceId = (InstanceId, noteIndex)`：一次具体音符播放。
- `ParameterId`：由作用域 + 轨道名 + 片段名 + 参数名组成的稳定字符串，例如 `track:Piano:volume`。

同一 Music IR 对象可被多个 Clip 实例引用，每个实例拥有独立的 `PerformanceExpression`：

| 通道 | 单位 | 范围 | MIDI 可表达 |
| --- | --- | --- | --- |
| velocity | 力度乘数 | 0.05..2.0 | 是 |
| timing | 全音符偏移 | -1/2..1/2 | 是（受 PPQ 量化） |
| gate | 时值乘数 | 0.1..2.0 | 是 |
| pan | -1..1 | -1..1 | 否，逐音符 pan 在 MIDI 中诊断后忽略 |

表情通道必须是 Relative 域、extent = 1 的曲线，采样点为音符在实例内的相对位置。

### 11.4 自动化绑定

```text
AutomationBinding
├── target: ParameterTarget（scope: track/clip/master, parameter, unit, range）
├── curve: TemporalId
├── domain: musical/relative
├── mode: replace/add/multiply
└── enabled
```

组合语义：

```text
base = replace ? replaceValue : (hasAdd ? 0 : 1)
combined = (base + Σ addValue) * Π multiplyValue
```

同一参数、同一时间域上出现多个 `replace` 是 `automation-conflict` 错误；`add` / `multiply` 可以与 `replace` 共存。曲线关键帧值必须落在参数范围内；组合结果越界时后端夹紧并产生诊断。

### 11.5 后端能力诊断

MIDI 后端只导出 musical 域、track 作用域的自动化；以下情况必须产生诊断，不能静默丢弃：

- master / clip 作用域自动化；
- relative 域自动化（应由表情通道表达）；
- 无 CC 映射的参数名；
- 逐音符 pan；
- 无法精确映射到 tick 的 timing 值；
- 组合值越界。

CC 编号、PPQ、通道分配、GM Program 只存在于 MIDI 后端，不进入通用 IR。

### 11.6 阶段边界

- 音乐程序合成：源码、自然语言、DAW/可视化前端，产出定义与意图。
- 确定性编译：Music/Temporal/Production IR、诊断、增量重算、导出。
- 音频运行：DSP、实时调制、空间音频、采样域求值。

V0.0.3 只实现第二阶段的基础时序语义；实时调制与音频运行不在本阶段。

## 12. V0.1.0 文本语言

### 12.1 结构

一个 `.mpl` 文件 = 一个 project + 若干声明。project 必须是第一个声明。

```text
project Reference {
  tempo = 160 bpm;
  meter = 4/4;
  key = F minor;
}

asset GrandPiano = instrument("GrandPiano");
asset SubBass = instrument("SubBass");

let A = melody {
  F4: 1/8, Ab4: 1/8, C5: 1/4
};

curve Crescendo on relative = linear(0.55, 1.15);
curve Volume on musical for 4 bars = linear(0.55, 1.0);

track Piano {
  asset = GrandPiano;
  role = melodic;
}

section Intro(4 bars) {
  clip Piano "A" {
    play A;
    velocity = Crescendo;
  }
}

arrangement {
  play Intro;
}

automation Piano.volume = Volume;

export midi("reference.mid");
```

完整语法见 `docs/grammar.ebnf`，类型与单位规则见 `docs/type-system.md`。

### 12.2 声明顺序

- project 必须最先出现。
- let / curve / asset / track 必须先声明后使用。
- section 可以引用已声明的 track 与 let/curve。
- arrangement / export 必须在最后阶段之前声明；顺序不影响 lowering。
- 所有命名声明共享一个全局命名空间。

### 12.3 音乐字面量

- `melody { Pitch: Duration, ... }`：顺序排列单声部。
- `notes span <Duration>? { Pitch @ Position for Duration [vel N], ... }`：显式偏移、复声部、休止。
- 裸有理数在音乐字面量中表示全音符分数；`bars` / `beats` / `whole` 是显式时长单位。

### 12.4 表达式

- 不可变 `let`；V0.1.0 只允许 Fragment 或 Curve 值。
- 调用内建算子；没有用户函数、模块、算术表达式、控制流。
- `transpose(A, 12 st)`、`harmonize(A, [2, 4])`、`syncopate(A, 1/8)`、`repeat(A, 8)`。
- 曲线：`constant/linear/step/smoothstep/keyframes` 与 `shift/stretch/reverse/repeat`。

### 12.5 Section / Track / Clip / Arrangement

- track 声明音源资产与角色。
- section 有固定时长；内部每个 track 可有多个 clip，可用 `at <time>` 指定相对 section 起点的偏移。
- clip 指定 `play` 表达式与可选 velocity/timing/gate/pan 表情曲线。
- arrangement 按顺序播放 section；section 起点由前面 section 的时长累加。
- clip 超出 section 时产生警告并允许与后续 section 重叠。

### 12.6 自动化

```text
automation Piano.volume = Volume;
automation Piano.modulation = VolumeX2 add;
automation master.volume = reverse(Volume);
```

作用域为 track 或 master；参数来自 `ParameterRegistry`；模式为 replace/add/multiply（默认 replace）。V0.1.0 要求自动化曲线为 musical 域。

### 12.7 导出

```text
export midi("reference.mid");
export wav("reference.wav");
```

同一程序可以有多个导出声明，顺序不影响语义。`wav` 由 Reference Audio Backend 渲染；`midi` 由 MIDI Backend 导出。

### 12.8 编译命令

```text
mplc check reference.mpl
mplc check reference.mpl --json
mplc build reference.mpl --output build
mplc build reference.mpl --output build --sample-rate 48000 --block-size 256 --channels 2 --tail 1.0
mplc build reference.mpl --output build --json
```

`check` 完成词法、语法、名称解析、类型/单位/时间域检查与 lowering 校验，但不写产物。`build` 额外生成 Music IR / Temporal IR / Production IR / Trace，以及声明的 MIDI/WAV。

### 12.9 明确不支持

用户定义函数、模块、生成算子、约束表达式、情绪/审美映射、实时 DSP、Physical/Sample 域曲线。保留扩展点，但 V0.1.0 不引入无语义契约的语法。

## 13. V0.2.0-beta：混音、处理器与音频执行

### 13.1 处理器声明

处理器是命名的 DSP 定义，不在声明处分配状态；每次 `insert` 在信号图中创建独立实例。

```text
processor Eq = biquad {
  mode = peaking;
  frequency = 1200 Hz;
  q = 1.2;
  gain = 4 dB;
};

processor Comp = compressor {
  threshold = -18 dB;
  ratio = 4;
  attack = 10 ms;
  release = 120 ms;
  makeup = 3 dB;
};

processor Hall = reverb {
  room = 0.75;
  damping = 0.35;
  mix = 0.8;
};
```

| 类型 | 参数（单位，范围） |
| --- | --- |
| `gain` | gain（dB，-96..24） |
| `pan` | pan（标量，-1..1） |
| `biquad` / `eq` | mode（必填）、frequency（Hz，20..20000）、q（0.1..20）、gain（dB，-24..24） |
| `compressor` | threshold（dB，-80..0）、ratio（1..20）、attack/release（秒）、makeup（dB，0..24） |
| `delay` | time（秒，0..2）、feedback（0..0.95）、mix（0..1） |
| `reverb` | room、damping、mix（0..1） |
| `limiter` | ceiling（dB，-24..0）、attack（秒，0.001..0.05）、release（秒，0.01..2） |

`biquad` 的 mode 为 `lowpass | highpass | peaking | lowshelf | highshelf`；其他类型不接受 mode。未给出的参数取描述符默认值。参数单位不匹配、未知参数、越界值都是编译错误。

`limiter` 的 `attack` 是前视时间，也是处理器内部延迟；它不可被自动化（`sema-processor-automation`）。Limiter 只保证采样峰值 ceiling，不保证 True Peak；母带反馈会独立测量 dBTP。

### 13.2 通道条、Bus 与 Master

```text
bus Reverb {
  insert Hall;
  gain = -4 dB;
}

track Piano {
  asset = GrandPiano;
  role = melodic;
  gain = -2 dB;
  pan = -0.15;
  insert Eq;
  insert Comp;
  send 0.3 to Reverb;
  sidechain Comp from Drums;
}

master {
  insert MasterComp;
  gain = -1 dB;
}
```

语义：

- `insert` 顺序即处理器链顺序；同一处理器可在不同通道条中实例化多次。
- `send <0..1> to <bus>` 在后推子（Track Gain/Pan 之后）把信号送入 Bus。
- 所有 Track 与 Bus 输出求和进入 Master；Master 链之后是 Master Gain/Pan 与 Output。
- `sidechain <processor> from <track>` 的键信号取源轨 Source 输出（pre-insert、pre-fader）。
- 编译期解析声明顺序：处理器与 Bus 必须先于引用它们的 Track/Automation；Master 块在全部声明之后处理。

### 13.3 处理器参数自动化

```text
automation Piano.Eq.frequency = FilterCurve;
automation Reverb.Hall.mix = HallDepth;
automation master.MasterComp.threshold = DuckCurve;
```

- 两段目标 `<strip>.<parameter>` 仍是 Track/Master 的 `volume`/`pan`（V0.1.0 规则）。
- 三段目标 `<strip>.<processor>.<parameter>` 表示处理器参数；`<strip>` 可以是 Track、Bus 或 `master`。
- 目标处理器必须已插入该通道条；参数必须属于该处理器类型；曲线必须是 musical 域并落在参数描述符范围内。
- 处理器参数在块边界求值、块内插值；滤波器截止频率额外做 5 ms 一阶平滑。

### 13.4 信号图与执行顺序

```text
TrackSource → Processor… → Track Gain → Track Pan → MasterMixer
                               └── send gain → BusMixer → Processor… → Bus Gain/Pan → MasterMixer
MasterMixer → Master Processor… → Master Gain → Master Pan → Output
Sidechain: TrackSource → Compressor.key
```

- 节点按拓扑序执行；确定性拓扑排序（同层按节点 id）。
- 本阶段拒绝所有环路，诊断码 `graph-cycle`。未来带明确最小延迟的合法反馈必须由 Delay 节点显式声明，本阶段不实现。
- 普通音频端口为 stereo；mono 源在 Source 升为 stereo；stereo→mono 连接拒绝（`graph-channel-mismatch`）。
- 每个处理器实例在渲染开始 `prepare/reset`；块间保持状态，重渲染从头复位。

### 13.5 渲染终止

```text
base = max(logicalExtent, contentExtent + processorTail)
renderExtent = min(base + tailSeconds, base + maxTailSeconds)
```

- `logicalExtent` 由 arrangement 总时长决定，包含有意留白，不得因最后音符提前结束而删除。
- `contentExtent` 是最后音符结束；`processorTail` 是 Delay/Reverb 等有限状态的衰减长度。
- `Fixed` 按 renderExtent 渲染；`SilenceThreshold` 可在 logical/content 边界之后裁剪静音，但不得早于 `logicalExtent`。
- 无限状态处理器由 `maxTailSeconds` 截断并产生 `audio-tail-capped` 诊断。

### 13.6 范围边界

本阶段不实现：SoundBoxer Adapter、GUI、实时音频设备、完整插件宿主、AI/Taste、空间音频、通用 DSP 语言、audio-rate modulation。

### 13.7 生产目标不在文本语法内

母带响度目标、峰值约束、容差与迭代预算属于生产配置，不进入 `.mpl` 文本语法：

```text
mplc master examples/production.mpl --target-lufs -14 --peak-ceiling -0.5
```

文本语法只描述音乐程序、混音结构与处理器参数。分析结果与生产目标由 CLI/API 提供，避免把交付约束混入作品语义。

## 14. V0.3.0-alpha：用户函数与类型化求值

### 14.1 函数

```text
fn motif(root: Pitch, step: Int) -> Fragment context key, meter {
  let a = note(root, 1/8, 96);
  let b = diatonicTranspose(a, step);
  return sequence(a, b, a);
}
```

- 函数是纯函数；参数与返回值显式类型。
- 局部 `let` 不可变；允许遮蔽外层，但同一作用域重名是错误。
- `return` 必须是函数体最后一条语句；不支持提前 return。
- 禁止修改全局 Project、执行 I/O、声明 Track/Section/Export。
- 函数可读取全局 `let`/`curve`，但读取是只读依赖；全局 `let` 现在可持有标量、集合与音乐值。

支持类型：

```text
Fragment, Curve, Duration, Position, Pitch, Int, Number, Scalar,
Bool, String, Key, [T]
```

### 14.2 上下文

函数必须显式声明它读取的音乐上下文：

```text
fn riff(root: Pitch, variant: Int) -> Fragment context key, meter { ... }
```

- `diatonicTranspose`/`harmonize` 需要 `context key`；`syncopate` 需要 `context meter`。
- 调用带上下文的函数时，调用者也必须声明对应上下文；上下文不得隐式穿透。
- 全局表达式仍使用 Project 的 Key/Meter。
- 函数缓存键包含参数、Key、Meter 与函数定义版本。

### 14.3 控制流与集合

- `Bool`、`true`/`false`、`!`、`&&`、`||`、比较运算。
- `if condition { expr } else { expr }` 是表达式，两个分支类型必须一致。
- `for x in collection { expr }` 产生同类型集合。
- `range(start, count)` 产生 `[Int]`，count 在 [0, 1024]。
- `length`、`round`、`ratio(n, d)`。
- `sequenceAll`/`parallelAll` 把 `[Fragment]` 合成为 Fragment。
- 算术：`+ - * / %`；Int 与 Int 产生 Int，其他数值产生 Number；除零是编译错误。
- 集合字面量与 for 结果有 1024 上限；总循环迭代 8192；函数调用深度 32；总调用 4096。

### 14.4 音乐原语

| 原语 | 类型 | 语义 |
| --- | --- | --- |
| `note(pitch, duration, velocity)` | `Pitch, Duration, Int -> Fragment` | 单音片段 |
| `rest(duration)` | `Duration -> Fragment` | 空内容但保留 span |
| `rotate(fragment, amount)` | `Fragment, Duration -> Fragment` | 在 span 内循环移位 onset；越界音符在 span 末尾截断 |
| `random(seed, index)` | `Int, Int -> Number` | 显式种子、无全局状态的 [0,1) 随机数 |
| `ratio(n, d)` | `Int, Int -> Duration` | 精确有理时值 |

`rotate` 是周期/相位原语，不是艺术性 Delay，也不参与处理器延迟补偿。

### 14.5 求值、缓存与身份

- Lowering 使用类型化求值：函数返回 Music IR 对象 id，不展开为扁平音符。
- 同一函数 + 相同参数 + 相同上下文 + 相同函数版本 → 缓存命中，返回同一 Music IR 对象。
- 不同参数 → 不同 Music IR 对象；Clip 仍是独立播放实例。
- 函数版本是函数签名的结构哈希（参数、类型、上下文、函数体表达式）。
- 每次调用都记录 Trace：函数、调用位置、实例、深度、实参、上下文、缓存命中、创建对象与算子。
- 本阶段缓存只在一次编译内生效；跨编译的增量 Lowering 不在 alpha 范围。

### 14.6 明确不支持

递归、无限循环、全局可变状态、任意提前返回、I/O、函数值、泛型、模块、错误处理、并发。资源上限是硬错误，不会静默截断。

## 15. V0.3.0-beta：Meter Map、Rhythm 与节奏代数

详细规范见 `docs/meter-map-spec.md` 与 `docs/rhythm-semantics.md`。

### 15.1 变拍号

Section 可以声明自己的拍号与节拍分组：

```text
section A(4 bars) meter 7/8 grouping 2+2+3 {
  meter 5/8 at 2 bars grouping 2+3;
  clip Guitar { play RiffA; }
}
```

- Section 未声明拍号时使用 project 初始拍号，不继承前一段落；这保证同一 Section 重复播放时内容稳定。
- Section 内 `meter ... at ...` 的位置相对 Section 起点，必须落在小节边界；拍中变拍号不支持。
- Section 长度必须落在小节边界。
- Arrangement 按播放顺序拼接全局 Meter Map；Production IR 与 Trace 记录每个拍号事件及 grouping。
- MIDI 后端为每个事件输出标准时间签名元事件；MIDI 没有标准 grouping 元事件，grouping 保留在 Production/Trace 与 `describe()`。

### 15.2 度量单位与锚点

- `bars`、`beats` 是度量单位，Lowering 在“MeterMap + anchor”上下文中解析。
- 正整数 `bars` 逐小节边界行走，anchor 必须是小节起点；分数/负数 `bars` 按 anchor 处小节长度线性缩放。
- `beats` 使用 anchor 处生效拍号的拍长。
- `whole` 与裸有理数仍是绝对全音符时值。
- 全局 let 的 anchor 为 0，上下文为 project 初始拍号；Section 内容的 anchor 为 Section 局部坐标；Clip `play` 的 anchor 为 Clip 的局部 offset。
- 函数缓存键包含度量上下文（MeterMap 身份 + anchor），不同 Section 的同一函数不会错误共享结果。

### 15.3 Rhythm 类型

```text
let R3 = rhythm {
  hit 1/8 accent 2,
  hit 1/8,
  hit 1/16,
  rest 1/16
};
```

- `Rhythm` 是独立、类型化、可序列化的音乐值；包含逻辑长度、起音位置、持续时间、重音属性与事件 origin。
- 休止与逻辑留白保留；Rhythm 不含音高、乐器、MIDI 通道。
- Rhythm 与 Fragment 共用 Music IR 的 `ObjectId` 空间，但求值结果类型不同；`evaluateRhythm` 只接受 Rhythm 节点。

### 15.4 节奏代数

| 算子 | 语义 |
| --- | --- |
| `sequence(a, b, ...)` | 顺序拼接（Fragment/Curve/Rhythm 按首参类型分派） |
| `overlay(a, b, ...)` | Rhythm 从 0 叠加，保留重叠 |
| `repeat(r, n)` | 周期复制，n ∈ [1, 256] |
| `rotate(r, amount)` | 周期内相位平移，跨边界事件精确切分 |
| `shift(r, amount)` | 线性平移，可为负；不环绕 |
| `stretch(r, factor)` | onset/duration/逻辑长度乘以正有理数 |
| `subdivide(r, n)` | 每个事件切成 n 份，n ∈ [1, 64]，accent 复制 |
| `accent(r, levels)` | 起音按 onset 顺序循环应用 accent levels |
| `realign(r, period)` | 精确扩展到 period 的整数倍 |
| `quantize(r, grid)` | onset 吸附到最近 grid 点，平局向上 |
| `period(r)` / `commonPeriod(a, b)` | 逻辑长度 / 有理数最小公倍数 |
| `metricAccent(r)` | 用当前 MeterMap 的层级设置 accent（需 `context meter`） |
| `metricWeight(offset)` | 返回该位置的层级 3/2/1/0（需 `context meter`） |
| `onsetSet(r, unit)` | 把起音归约为给定 unit 时值的纯起音集合，丢弃休止 |

共同周期：`lcm(3/8, 7/8) = 21/8`。分子/分母上限 `1e9`；超出给出 `rhythm-period-overflow`，不无界展开。

### 15.5 Rhythm 与音高

| 算子 | 语义 |
| --- | --- |
| `zip(r, pitches, velocity)` | 严格一一映射；起音数必须等于音高数 |
| `cycle(r, pitches, velocity)` | 循环填充音高；休止不消耗音高 |

- 力度 = `clamp(base + accent * 8, 1, 127)`。
- 负 onset 不能进入 Fragment 时间轴，诊断 `rhythm-pitch`。
- 组合结果记录 `rhythmOrigin` 与 `rhythmEventIndex`，可逐音追溯到 Rhythm 事件。
- 不提供隐式“补音/丢音”规则；必须显式选择 zip 或 cycle。

### 15.6 跨声部节奏关系

| 算子 | 语义 |
| --- | --- |
| `intersect(a, b, [grid])` | 起音交集 |
| `union(a, b, [grid])` | 起音并集 |
| `difference(a, b, [grid])` | 起音差集 |
| `complement(r, span, grid)` | 网格补集 |
| `accentFollow(source, target, [grid])` | 目标起音跟随来源 accent |

- 这些算子处理**起音集合**，不处理完整持续区间；长音不会吸收另一声部的起音。
- 无 grid 时按精确有理 onset 匹配；有 grid 时先吸附到最近 grid 点。
- 结果逻辑长度 = 两个 Rhythm 的共同周期；complement 的结果长度为 span。
- 不引入通用跨声部约束求解器。

### 15.7 函数身份与缓存

- Music Value（`ObjectId`）、函数调用实例（调用点 + 实例序号）、演奏实例（`InstanceId`）保持分离。
- 同参同定义同上下文的函数调用共享 Music Value；每次调用仍记录独立 invocation。
- 每个 Clip 仍是独立 Performance Instance，拥有独立 `PerformanceExpression`；共享 Music Value 不合并表情或自动化。
- 函数定义版本变化会改变缓存键；调用链上的定义变化会改变生成结果。
- 度量上下文（MeterMap 身份 + anchor）进入缓存键；无关 Section 的拍号变化不会改变另一 Section 的函数结果。
- 随机仍为显式 `random(seed, index)`，无全局状态。

### 15.8 本阶段明确不支持

LLM 集成、情绪映射、Taste 学习、通用约束求解、完整泛型、任意递归、完整插件宿主、SoundBoxer Adapter、大规模 DSP 扩展、拍中变拍号、完整变速系统。

## 16. V0.3.0-rc：音高、和声、声部关系与模块

详细规范见 `docs/pitch-harmony-spec.md`、`docs/voice-relations-spec.md`、`docs/module-spec.md`。

### 16.1 Beta 语义修正

- 所有正 `bars` 都要求小节锚点；整数逐小节行走，分数按锚点小节长度缩放；负数线性位移。
- Rhythm 跨周期切分使用 `continuation`：不自动 retrigger；`retrigger(rhythm)` 显式重新触发。
- Tie 未实现；跨周期持续音在边界截断。
- MIDI 只输出时间签名；grouping 在 Production/Trace/describe，不伪造元事件。

### 16.2 音高与和声类型

| 类型 | 语义 |
| --- | --- |
| `PitchClass` | 0..11，无八度/拼写身份 |
| `PitchClassSet` | 去重排序的 pitch class 集合 |
| `Chord` | 和弦符号：根音、质量、显式 interval set、slash bass |
| `Harmony` | 和声时间线 |
| `Voicing` | 低到高的具体排列 + 状态/移动/交叉/完整信息 |

和弦字符串支持大三/小三/减/增、sus、5、6、7、maj7、m7、m7b5、dim7、mMaj7、
9/11/13、变化音与 `/Bass`。

### 16.3 和声时间线

```text
let H = harmony span 10 whole {
  "Dm9" @ 0 whole for 7/4 whole,
  "G7b9" @ 7/4 whole for 7/4 whole
};
```

- 事件覆盖 `[start, start+duration)`；
- 允许重叠/空白；`eventAt` 取 start 最大者，同 start 后者优先；
- 边界右连续；gap 返回空和弦；
- 支持 `chordAt(h, at)` 与 `chordAtBar(h, bar)`（后者需 `context meter`）；
- 和声不直接生成音符，必须与 Rhythm + 明确音高策略组合。

### 16.4 排列与声部连接

```text
let V0 = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let V1 = voiceLead(voicingNotes(V0), chord("G7b9"), D3, D5, 7, 12, false);
```

- `voicing`：声部数、音域、转位、最大间距；
- `voiceLead`：硬约束（声部数/音域/和弦音/顺序/间距/最大移动）+ 总移动距离最小；
- 状态：严格满足 / 容差内满足 / 不满足 / 后端能力不足 / 预算耗尽 / 可证明不可满足；
- 预算耗尽不声明无解；
- 查询：`voicingNotes`、`voicingStatus`、`voicingMovement`、`voicingCrossings`、
  `voicingComplete`。

### 16.5 关系分析

- Analyze：`interval`、`pitchClass`、`pitchRange`、`fragmentRange`、`chordContains`、
  `commonTones`、`voiceDistance`、`voiceCrossings`、`noteOverlap`；
- Validate：`voicingStatus`、`voicingComplete`、`parallelPerfects`；
- Construct：`voicing`、`voiceLead`、`zip`/`cycle`、stdlib 函数；
- 非和弦音、平行运动、声部交叉默认不构成违规，只有显式规则才成立。

### 16.6 最小模块

```text
module StdHarmony;
import "std/voicing.mpl";
export fn helper(...) -> Fragment { ... }
```

- `import "path";` 在 project 前；
- 导出名无前缀进入导入者命名空间；私有名被链接器 mangle；
- 循环导入、缺失导入、导出冲突、库内不支持声明均有诊断；
- 模块内容哈希/imports/exports 进入 `Compilation::modules` 与 Trace；
- 标准库位于 `stdlib/`，是普通 MPL 模块。

### 16.7 乐器能力

- `InstrumentCapability`：音域、复音、开放弦、品数、弦/品位置；
- 标准吉他配置；不实现完整指法求解器；
- 乐器约束不污染通用 Pitch/Chord 语义。

### 16.8 本阶段明确不支持

LLM 集成、情绪映射、Taste 学习、通用约束求解、完整泛型、任意递归、完整插件宿主、
SoundBoxer Adapter、大规模 DSP、完整微分音/调律、Tie、完整模块命名空间/包管理器。
