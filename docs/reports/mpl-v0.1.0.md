# MPL V0.1.0 开发报告 — Language Frontend

日期：2026-11-24

结论：完成 `.mpl` → Lexer → Parser → AST → Resolver → Typed HIR → Core Lowering → MIDI 的完整编译闭环。Core/Suite/Temporal 三个源程序的 MIDI 输出与等价 C++ API 构造结果**字节级一致**。98 项测试通过，新增 AST、Reference IR、Reference Trace 与 MIDI 黄金输出。

---

## 1. 交付物

| 交付物 | 路径 |
| --- | --- |
| 编译器 CLI | `mplc check` / `mplc build` / `mplc ast` |
| 语言规范 | `docs/language-spec.md` |
| EBNF | `docs/grammar.ebnf` |
| 类型系统 | `docs/type-system.md` |
| 前端架构 | `docs/frontend-architecture.md` |
| Reference Program | `examples/reference.mpl` |
| 回归示例 | `examples/core.mpl`、`examples/suite.mpl`、`examples/temporal.mpl` |
| 结构化诊断 | 人类格式 + `check --json` |
| ADR | `ADR-0009`（前端流水线）、`ADR-0010`（稳定实例身份） |
| 测试与黄金 | 98 项测试、15 个黄金文件 |

---

## 2. 语言设计摘要

优先采用声明式 + 函数式混合：

- `project` 声明 tempo / meter / key。
- `asset` 声明 instrument/sample。
- `let` 不可变绑定 Fragment 或 Curve。
- 音乐字面量：`melody { Pitch: Duration }` 与 `notes span ... { Pitch @ Position for Duration vel N }`。
- 内建算子：transpose / reverse / stretch / repeat / sequence / parallel / diatonicTranspose / harmonize / syncopate。
- 曲线：`constant/linear/step/smoothstep/keyframes` 与 `shift/stretch/reverse/repeat`。
- `track`、`section`、`clip`（支持 `at <time>` 相对偏移、velocity/timing/gate/pan 表情）、`arrangement`。
- `automation Track.parameter = curve [mode]`。
- `export midi("...")`。

Parser 不执行音乐逻辑；所有 Core 对象只在 Typed HIR 通过后构造。

---

## 3. 编译器前端

### 3.1 流水线

```text
Lexer → Token[]
Parser → AST + SourceSpan
Resolver → SymbolTable + exprSymbols
TypeChecker → Typed HIR（exprTypes / exprConstants / symbolTypes）
Lowering → Project(MusicIR + TemporalIR + TrackSpec/Automation)
```

- Lexer：关键字、标识符/音高、整数/有理数/小数、字符串、注释、UTF-8 BOM。
- Parser：递归下降，声明/语句边界同步，单次错误不会导致无限级联。
- AST：arena + source span；`mplc ast` 输出 AST JSON；黄金测试覆盖 `examples/core.mpl`。
- Resolver：全局命名空间、声明前使用、重复/未知名称。
- Sema：类型、单位、参数数量、时间域、参数范围、自动化冲突、section 边界。
- Typed HIR：与 AST 表达式对齐的类型、符号、常量注解；Lowering 不重复类型推断。

### 3.2 类型与单位

- Pitch / Interval / ScaleDegree / Duration / Position / Tempo / Frequency / Gain / PhysicalTime / Scalar / String / Fragment / Curve / List / Asset / Track / Section。
- 音乐时间保持全音符有理数；`bars` / `beats` 用 project meter 精确换算。
- `Hz` / `dB` / `ms` / `s` 可解析为 Frequency / Gain / PhysicalTime；V0.1.0 没有消费它们的算子，因此会得到类型错误而不是静默忽略。
- Relative 曲线归一化 extent = 1；表情通道必须是 Relative；track/master 自动化必须是 Musical。
- Physical/Sample 曲线域被明确拒绝。

### 3.3 诊断

```text
file:line:col: error[type-unit]: tempo requires the 'bpm' unit
  expected: number bpm
  actual:   integer
```

`mplc check --json` 输出稳定字段：code、severity、file、line/column、offset/length、message、expected、actual、related、suggestions。

---

## 4. Core API 映射与最小修改

### 4.1 映射

| 语言 | Core |
| --- | --- |
| `melody` / `notes` | `MusicIR::melody` / `notes` |
| transpose 等 | `MusicIR::transpose` 等 |
| 曲线声明 | `TemporalIR::constant/linear/…` |
| 曲线变换 | `TemporalIR::shift/stretch/reverse/repeat` |
| track | `Project::addTrack`（空轨道） |
| clip + `at` | `Project::addClip` + `ClipSpec.instanceId/expression` |
| automation | `Project::automation()` + `AutomationBinding` |
| export | `MidiBackend` + CLI 产物 |

### 4.2 Core 修改

1. `Project::addTrack(name, instrument, role)` 允许先声明空轨道，再由 arrangement 添加 clip。
2. `ClipSpec` 增加可选 `InstanceId`；Lowering 使用前端提供的稳定 id，未提供时仍顺序分配。
3. `validate-production` 增加重复实例 id 诊断。
4. Section 内容超出时长从错误改为**警告**，允许与后续 section 重叠（与既有 Suite 案例的真实行为一致）。

没有重构 V0.0.3 的 Music/Temporal/Production 语义。

### 4.3 稳定身份

实例 key：

```text
track:<track>|section:<section>|play:<n>|clip:<clip>
```

- 以 FNV-1a 映射为 32-bit `InstanceId`。
- 冲突报 `sema-instance-id-collision`。
- 重命名 track/section/clip 会改变身份；插入同 section 的更早播放会改变后续 `n`；插入其他 section、编辑音符/曲线不影响已有身份。
- 源码行号不参与身份；`ObjectId` 仍是单次编译内 IR 身份。

---

## 5. 验证

### 5.1 测试

- **98/98 测试通过**（Debug 与 Release）。
- CTest 1/1 通过。
- 新增覆盖：
  - Lexer：关键字、rational、注释、字符串、未闭合字符串、UTF-8 BOM。
  - Parser：声明/span、错误恢复。
  - AST 黄金（`core.ast.json`）。
  - 名称解析：重复、未知名称。
  - 类型/单位：Tempo 缺单位、Hz/dB/ms 误用、未知算子。
  - 时间域：physical 曲线、relative 自动化、musical 表情。
  - 自动化：冲突、范围、表情 extent。
  - section 越界警告。
  - 稳定实例 id 跨编译一致性。
  - 机器可读诊断字段。
  - Reference IR/Trace/MIDI 黄金。
  - Core/Suite/Temporal 源程序与 C++ API 的 MIDI 字节一致性。

### 5.2 源程序与 Core API 等价

| 案例 | 源程序 | C++ API | MIDI |
| --- | --- | --- | --- |
| Core | `examples/core.mpl` | `buildCoreDemoProject()` | 字节一致 |
| Suite | `examples/suite.mpl` | `buildSuiteDemoProject()` | 字节一致 |
| Temporal | `examples/temporal.mpl` | `buildTemporalDemoProject()` | 字节一致 |

三个案例的 Music IR 对象数分别为 5 / 21 / 9，音符数 40 / 416 / 47，时长 4 / 24 / 8 小节。

### 5.3 Reference Program

`examples/reference.mpl`：

- 24 小节、5 轨、11 clip、411 音符。
- 4 段落（Intro / Drop A / Drop B / Outro）。
- 5 种角色；动机发展、和声、鼓组、表情、曲线与自动化。
- 21 个 Music IR 对象、6 条曲线、3 个自动化绑定。
- MIDI 5121 bytes；黄金覆盖 Music/Temporal/Production/Trace/MIDI。

黄金文件共 15 个：此前 9 个 + Reference 5 个 + AST 1 个。

---

## 6. 已知局限

1. 一个文件一个 project；全局命名空间，let/track/curve/section 不能同名。
2. 只支持声明前使用，没有前向引用。
3. 不支持用户函数、模块、算术表达式、条件/循环、生成算子或约束。
4. package/module 导入未实现。
5. track/master 自动化在语言层要求 Musical 曲线；Relative track 自动化仍可用 Core API 表达。
6. Frequency/Gain/PhysicalTime 字面量可解析但没有消费算子；使用处会产生类型错误。
7. Section 越界是警告；clip 会与后续 section 重叠。
8. 实例身份受重命名和同 section 播放序号影响，尚未支持显式 `as <name>`。
9. Core 诊断（如 `validate-production`）没有 SourceSpan；前端已覆盖常见错误，但少量 Core 诊断只能以无位置形式显示。
10. 每次 `build` 都从头编译；没有源码级增量缓存。
11. `check --json` 只输出前端诊断；build 的 Core/后端诊断目前是人类可读格式。
12. 没有实时 DSP、空间音频、情绪/审美策略或 AI。

---

## 7. 验收对照

| 要求 | 状态 |
| --- | --- |
| check / build 命令 | 完成 |
| Lexer / Parser / AST / SourceSpan | 完成 |
| SymbolTable / Name Resolution | 完成 |
| Semantic Analysis / 单位 / 时间域 | 完成 |
| Typed HIR / Core Lowering | 完成 |
| 类型系统与 ParameterRegistry | 完成 |
| 稳定实例身份策略 | 完成（ADR-0010） |
| 人类 + JSON 诊断、错误恢复 | 完成 |
| 保留 83 项旧测试 | 完成 |
| 新增前端/诊断/等价/黄金测试 | 完成（98 项） |
| Core/Suite/Temporal `.mpl` | 完成 |
| 源程序 vs Core API 语义一致 | 完成，MIDI 字节一致 |
| Music/Temporal/Production/Trace/MIDI 黄金 | 完成 |
| Reference Program | 完成 |
| 语言规范 / EBNF / 类型系统 / 前端架构 / ADR / 报告 | 完成 |
| 不实现 LLM / DSP / 约束 / 生成算子 | 遵守 |

Phase 1 的编译闭环已成立。后续扩展应优先通过标准库、显式扩展接口和真实音乐案例驱动，而不是继续扩大 Core 前置范围。
