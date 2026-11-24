# MPL Core V0.0.3 开发报告

日期：2026-11-24

范围：Temporal Semantics & Core Stabilization

结论：完成时变属性、Definition/Instance 分离、自动化绑定、MIDI CC 导出与 Trace v3；83 项测试全部通过，Debug / Release / CTest 一致；新增 5 个 Temporal Demo 黄金产物。未引入 Lexer/Parser、实时 DSP、约束求解或 AI。

---

## 1. 审查发现与实际修改

### 1.1 Rational 比较依赖浮点

**事实**：V0.0.2 的 `operator<` 使用 long double 交叉相乘。音乐规模数值通常安全，但不满足“精确比较”要求。

**修改**：改为 Euclid 连分数比较，全程整数运算，不产生溢出乘积。新增极端分数、负数、排序一致性、溢出边界测试。

**结果**：`1/3 < 1/2`、`(MAX-1)/MAX > (MAX-2)/(MAX-1)` 等均精确；大数比较不再经过浮点。

### 1.2 Note::origin 与多源溯源

**事实**：`Note::origin` 只记录直接引入者；`derivation` 是 clip 级全依赖列表，无法直接回答某个音符经过哪条算子路径。

**修改**：

- 保留 `Note::origin`（直接来源）；
- Trace 为每个音符计算 `originChain`（clip 根到 origin 的确定性首路径）；
- 新增 `NoteInstanceId = (InstanceId, noteIndex)` 并在 trace 输出 `noteInstance`；
- harmonize 生成音仍以 harmonize 节点为 origin，原音保留输入来源。

**限制**：`originChain` 只给首路径，不枚举所有多源路径；`derivation` 仍列出完整依赖集合。

### 1.3 DiatonicTranspose 非调内音规则

**事实**：实现规则是“按拼写音级移动，并保留相对调内音的半音变化”，但文档与测试未完全说明。V0.0.2 测试中曾把 `C#4 +2` 误期望为 `F#4`。

**正确语义**（F minor）：

```text
B3  +1 → C#4   升高的四级 → 升高的五级
E4  +1 → F#4   升高的七级 → 升高的主音
C#4 +2 → E4    升高的五级 → 升高的六级
F4  -2 → Db4
```

**修改**：在 `operator-spec.md` 增加前置条件、输出保证、失败行为；测试覆盖上述四种情况与 C major 的普通音级移动。实现未改变。

### 1.4 高级算子语义契约缺失

**修改**：为 DiatonicTranspose / Harmonize / Syncopate 增加显式契约：

- 前置条件（上下文、参数范围、MIDI 音域）；
- 输出保证（音符数、偏移、时值、origin、span）；
- 失败行为（错误码）；
- 非目标（和声功能、声部进行、风格、张力）。

### 1.5 Production IR 缺少实例与表情

**事实**：V0.0.2 的 clip 只是对象引用 + 音符快照，无法表达“同一动机的多个独立演奏实例”。

**修改**：

- `ClipIR` 增加 `instanceId` 与 `PerformanceExpression`；
- 同一 Music IR 对象可被多个 clip 引用，实例 id 按 lowering 顺序确定性分配；
- 参数身份改为名称型 `ParameterId`。

### 1.6 缺少 Temporal IR

**修改**：新增独立 `TemporalIR` 值图：

- Constant / Linear / Step / Smoothstep / Keyframes；
- Shift / Stretch / Reverse / Repeat；
- 可序列化 `CurveSegment`，缓存与失效，JSON 输出；
- 支持 Musical 与 Relative 域；Physical / Sample 域建立类型边界但不求值。

### 1.7 MIDI 自动化与能力诊断

**修改**：

- 新增 MIDI CC 事件与序列化；
- 自动化按 Replace / Add / Multiply 组合后采样导出；
- master/clip 作用域、relative 域、未知参数、逐音符 pan、timing 量化均产生显式诊断；
- CC 编号、PPQ、通道、GM Program 仍只存在于 MIDI 后端。

### 1.8 Trace 缺少时序与实例

**修改**：Trace v3 增加：

- `temporal`：曲线节点、参数、求值段、求值统计；
- `automation`：参数、模式、域、曲线 id；
- `expressions`：每个 clip 实例的四条表情曲线；
- `provenance`：`instanceId`、`noteInstance`、`originChain`；
- `toTraceJson(false)` 可移除 pass 计时，用于确定性黄金输出。

---

## 2. Temporal IR 与时间域

### 2.1 时间域边界

| 类型 | 表示 | V0.0.3 |
| --- | --- | --- |
| `MusicalTime` | 全音符有理数 | 支持曲线 |
| `RelativeTime` | [0,1] 有理数 | 支持曲线 |
| `PhysicalTime` | 秒（double） | 仅类型边界 |
| `SampleTime` | 采样序号（int64） | 仅类型边界 |

曲线不使用 BPM 或采样率；物理时间换算仍由 `Duration::toSeconds` / 后端完成。

### 2.2 曲线语义

- 段 `[start,end)`，域外保持首/末值；
- Step 在段内保持 `from`，边界右连续；
- Linear / Smoothstep 使用显式公式；
- Reverse 对 Step 单独处理镜像边界；
- Shift 保持 extent，Stretch/Repeat 调整 extent。

### 2.3 缓存与增量

- Temporal 图按 id 拓扑求值并缓存；
- 修改曲线只失效该节点及下游；
- 测试验证：修改音量曲线后 Music IR 的求值次数与已求值对象数完全不变；
- 增量构造与从零构造的 Temporal / Production / Music JSON 及 MIDI 字节一致。

---

## 3. Definition / Instance 与自动化

### 3.1 身份

- `ObjectId`：Music IR 定义；
- `InstanceId`：Production IR 播放实例；
- `NoteInstanceId`：`(InstanceId, noteIndex)`；
- `ParameterId`：`track:Piano:volume` / `clip:Piano:Drop A:gain` / `master:volume`。

### 3.2 表情

| 通道 | 范围 | MIDI |
| --- | --- | --- |
| velocity | 0.05..2.0 | 支持 |
| timing | -1/2..1/2 全音符 | 支持，tick 量化 |
| gate | 0.1..2.0 | 支持 |
| pan | -1..1 | 不支持逐音符，诊断后忽略 |

相对时间采样点为 `note.offset / clip.duration`，因此对象时值改变后表情自动重映射。

### 3.3 自动化组合

```text
base = replace ? replaceValue : (hasAdd ? 0 : 1)
combined = (base + Σ add) * Π multiply
```

- 多个 Replace 同目标 → `automation-conflict`；
- Add / Multiply 可共存；
- 曲线值越界 → `automation-range`；
- 组合值越界 → 后端夹紧并产生 `midi-automation-range`。

---

## 4. Temporal Demo

在 Suite Demo 基础上新增 Temporal Demo：

- 同一动机 A 的三个独立播放实例：
  - 线性渐强；
  - smoothstep 非线性渐弱；
  - 固定力度 + 相对时间 humanize + gate + 逐音符 pan（不可导出）。
- 曲线复用与变换：
  - `Volume 4 bars` → `stretch ×2` → `modulation` Add；
  - `Volume 4 bars` → `reverse` → `expression` CC11。
- 跨小节自动化：4 小节音量、8 小节调制。
- 多重绑定：同一轨道三个不同参数绑定；面板覆盖 Replace / Add。
- BPM 160 → 120：音乐时长不变，物理时长 12s → 16s，CC tick 与力度不变。
- 后端诊断：逐音符 pan、timing 量化、master 自动化。
- 增量一致性：曲线修改后构造结果与从零构造一致。

指标：

| 项目 | 数值 |
| --- | --- |
| Music IR 对象 | 9 |
| Temporal 曲线 | 9 |
| 自动化绑定 | 4 |
| 轨道 / Clip / 音符 | 3 / 5 / 47 |
| 时长 | 8 小节 / 12 秒 @160 |
| MIDI | format 1、480 PPQ、4 track chunk、1555 bytes |

---

## 5. 测试与产物

命令：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
.\build\Release\mpl_tests.exe
ctest --test-dir build -C Release --output-on-failure
.\build\Release\mplc.exe temporal-demo --output build\v003
```

结果：

- **83/83 测试通过**（Debug 与 Release）。
- CTest 1/1 通过。
- 黄金输出 9 个：
  - Core Demo：Music IR / Production IR / MIDI；
  - Suite Demo：MIDI；
  - Temporal Demo：Music IR / Temporal IR / Production IR / Trace（无计时）/ MIDI。
- 测试支持 `--update-golden` 显式重写黄金输出，默认仍严格比较。

新增测试覆盖：

- 精确 Rational 比较、极端分数、排序；
- 时间域类型边界；
- 五类基础曲线、四类变换、边界保持；
- Temporal 缓存失效与依赖；
- 实例 id / 参数 id 稳定性；
- 相对表情随时值重映射；
- BPM 对音乐时间与物理时间的分离；
- Replace / Add / Multiply 组合；
- 冲突、范围、域、目标诊断；
- MIDI CC 值、通道、tick 与不支持能力诊断；
- 增量与全量构造一致性；
- 多源 originChain 与 noteInstance；
- 黄金输出。

---

## 6. 已知局限

1. 没有文本语言前端；`mplc` 仍只有内置案例命令。
2. 没有音频渲染、实时调制或采样级曲线求值。
3. Physical / Sample 域仅定义类型边界，不能创建曲线。
4. 只有单一 tempo / meter 事件，没有 tempo map 曲线。
5. 表情为 clip 级共享曲线，不是逐音符独立自动化。
6. MIDI CC 采样为 1/16 全音符、最多 2048 点；CC 编号是后端策略。
7. master/clip 作用域自动化、relative 域自动化、逐音符 pan 不在 MIDI 导出范围，只产生诊断。
8. `originChain` 只记录首路径；多源完整路径由 `derivation` + 多个 origin 共同表达。
9. 实例 id 依赖 lowering 顺序；重排轨道会改变编号（参数 id 不受影响）。
10. `Rational` 仍是 int64 有界精确值；极大值抛错而非降精度。
11. 没有生成式自动化、感知策略、Taste Profile 或空间音频实现；这些只有职责边界文档。

---

## 7. Phase 1 前端建议

1. 先实现最小语法：`project / asset / tempo / meter / key / let / section / track / automation / export`。
2. 单位安全：`Duration`、`Position`、`Interval`、`Gain`、`Frequency`、`Relative` 与字面量后缀；非法组合在语义分析期报错。
3. 时间语法必须显式选择域：`bars`/`beats` → Musical，`0..1` → Relative，秒/采样留给音频运行时。
4. 曲线语法降低到现有 TemporalIR 节点，不新增临时函数对象表示。
5. 文本层直接复用 `Project` / `MusicIR` / `TemporalIR` / `Compilation`，以 Core 为语义 oracle。
6. 诊断带源码位置；错误码延续 `pitch` / `meter` / `harmony` / `rational` / `temporal` / `automation-conflict` 等。
7. 黄金策略：AST 快照 + 两级 IR + Temporal IR + Production IR + Trace（无计时）+ MIDI hex。
8. 本阶段不引入生成算子、约束求解、AI 或实时 DSP；新理论维度优先通过 stdlib/扩展接口与真实案例驱动。

---

## 8. 验收对照

| 要求 | 状态 | 证据 |
| --- | --- | --- |
| 精确 Rational 比较与边界测试 | 完成 | Euclid 比较；极端分数/排序测试 |
| origin / derivation 多源溯源 | 完成 | `originChain`、`noteInstance`、多源测试 |
| Diatonic 非调内音规则 | 完成 | 契约文档；B3/E4/C#4 测试 |
| 三算子语义契约 | 完成 | `docs/operator-spec.md` 第 8 节 |
| Temporal IR 九类节点 | 完成 | Constant/Linear/Step/Smoothstep/Keyframes/Shift/Stretch/Reverse/Repeat |
| 时间域边界 | 完成 | 四类时间类型；Musical/Relative 曲线 |
| Definition/Instance/参数 id | 完成 | InstanceId、NoteInstanceId、ParameterId |
| 自动化作用域/模式/范围/冲突 | 完成 | 绑定模型、组合测试、冲突诊断 |
| 增量与 BPM 测试 | 完成 | 曲线失效、相对重映射、BPM 分离 |
| MIDI CC 与能力诊断 | 完成 | CC 导出、pan/master/量化诊断 |
| 综合 Temporal Demo | 完成 | 3 实例、9 曲线、4 绑定、47 音符 |
| 确定性 IR/Trace 黄金 | 完成 | 9 个黄金产物 |
| 理论边界文档 | 完成 | `docs/theory-boundaries.md` |

Phase 0 基础设施到此收束。后续新理论维度按约定通过标准库、扩展接口与真实音乐案例驱动，不继续扩大 Core 前置范围。
