# MPL 语言与核心语义规范（V0.1.0）

状态：本版本只有 C++ Core API，没有文本语法前端。本文描述的是前端必须遵守的语义契约；Phase 1 的 `.mpl` 语法只是这些对象与算子的另一种写法。

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
