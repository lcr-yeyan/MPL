# MPL Core V0.0.2 开发报告

日期：2026-11-24

范围：Phase 0.5 — Core Semantic Stress Test & Architecture Review

结论：完成 10 项审查与改造；61 项测试全部通过（Debug / Release / CTest）；黄金输出覆盖 Core Demo 的 Music IR、Production IR、MIDI 与 Suite Demo 的 MIDI。

---

## 1. 摘要

V0.0.1 建立了“音乐数据模型 → Music IR → Production IR → MIDI”的最小闭环。Phase 0.5 没有进入语法前端，而是用真实语义压力测试暴露并修正了四类问题：

1. **示例时值口径错误**：160 BPM、4/4 的 4 小节应为 6 秒。V0.0.1 的样例动机实际写了 4 个全音符（4 小节）却标注为 1 小节，导致 16 小节、24 秒。换算实现正确，样例与报告错误。V0.0.2 修正时值并加入端到端锁定。
2. **Rational 算术缺少溢出保护**：大数加/减/乘会静默溢出；现已改为受检算术并抛 `rational` 错误。
3. **Production IR 被 MIDI 细节污染**：通道、GM Program、percussion 标志已移出，改由 MIDI 后端计算；新增后端无关的角色/资产模型。
4. **缺少上下文依赖与逐音符溯源**：新增 Key/Meter 上下文依赖、增量失效、`Note::origin` 与 Trace 生成链。

同时新增三个实验算子（DiatonicTranspose、Harmonize、Syncopate）和一个包含 4 段落、5 轨道、11 clip、416 音符的综合案例。

---

## 2. 审查方法与事实分类

审查顺序：先读实现与测试，复算时间，再定位耦合点，最后才修改。

本文严格区分：

- **已验证事实**：由代码、测试或实际产物验证。
- **设计假设**：当前合理但未证明的选择。
- **未来扩展接口**：有意预留、尚未实现的能力。

---

## 3. 十项任务结果

### 3.1 时间系统审查

**事实**

- Music IR 以全音符有理数为单位；4/4 一小节 = 1 全音符。
- 160 BPM 下全音符 = 240/160 = 1.5 秒；4 小节 = 4 全音符 = 6 秒。
- V0.0.1 Core Demo 的动机时值为 `1/2+1/2+1+1+1 = 4` 全音符；Main 为 4 份该动机 = 16 全音符 = 16 小节；24 秒的计算与实现一致。
- “4 小节、24 秒”的不一致来自：动机被描述为 1 小节，但实际写了 4 小节；不是 `Rational`、PPQ 或 Tempo 换算错误。

**处理**

- Core Demo 动机改为 `1/8+1/8+1/4+1/4+1/4 = 1` 全音符；Main 恢复为 4 小节。
- 新增端到端断言：
  - Production 总时长 = 4 全音符；
  - 秒数 = 6.0；
  - PPQ=480 下 4 小节 = 7680 tick；
  - MIDI ledger 最后一次 note-off 位于 `@7680`。
- 补充三连音与复合拍号测试（见 3.6）。

**结论**：报告错误 + 示例数据错误；实现换算正确。已在文档中保留这次事故作为 ADR-0001 的依据。

### 3.2 Rational 规范化、溢出、非法分母、量化

**已验证**

- 规范化：约分、负分母统一、零分子归一为 `0/1`。
- 非法分母：构造与除法均抛 `rational` 错误。
- 溢出：新增受检加、减、乘；拒绝 `INT64_MIN` 量级；乘法先交叉约分再检查。
- 量化：Production IR 永远精确；MIDI 后端提供 `isExactAtPpq()`。
  - PPQ=480 时 `1/12` 全音符 = 160 tick，精确；
  - `1/7`、`1/9` 不可精确表示，必须四舍五入。

**测试**：`RationalDetectsOverflow`、`RationalRejectsZeroDenominator`、`MidiReportsPpqQuantization`、`MidiTicksFollowPpqAndMusicalTime`。

**设计假设**

- 音乐用途下 int64 足够；极端值抛错而不是引入大整数。
- `Rational::operator<` 仍用 long double 比较；对音乐规模数值精确，但超大分子/分母不是严格精确比较（已知局限）。

### 3.3 Music IR DAG、上下文依赖与增量失效

**已验证**

- 生成依赖：`inputs` 构成 DAG，算子只能引用已存在对象。
- 上下文依赖：`diatonic-transpose` / `harmonize` 依赖 key；`syncopate` 依赖 meter。
- `setKey` 只失效 key 相关节点及其下游；`setMeter` 同理。
- 增量与全量一致性：修改字面量后增量求值的 `MusicIR::toJson()` 与从零重建的等价图逐字节一致。
- 求值统计：一次 4 节点图初始求值 4 次；修改一个上游后仅重算 3 个受影响节点，独立节点命中缓存。

**测试**：`MusicIrTracksDependentsAndInvalidates`、`ContextInvalidatesOnlyDependentOperators`、`IncrementalEvaluationMatchesFullRebuild`、`MusicIrRecomputesDependentsAfterEdit`。

**设计假设**

- 失效采用线性扫描 + 下游 BFS，原型规模足够；性能索引留待后续。
- 上下文只有 Key / Meter；BPM、演奏法、音源能力未来可扩展为新的 aspect。

### 3.4 Production IR 后端无关性

**问题确认**

V0.0.1 `TrackIR` 含 `midiChannel`、`midiProgram`、`percussion`，并由 `assign-channels` Pass 赋值。PPQ 虽只在 `MidiOptions`，但通道/音色属于 MIDI 策略，污染了通用生产模型。

**改造**

- 删除上述三个字段。
- `TrackIR` 改为：`name`、`instrument`、`role`、`assetSlots`、`assetBindings`、`clips`。
- 通道/GM Program/Percussion 通道策略移入 `MidiBackend`（10 通道保留策略、按角色分配）。
- Pass 改为 `validate-production` + `validate-assets`，均不涉及 MIDI。
- 新增测试：Production IR JSON 不得出现 `midiChannel` / `midiProgram` / `percussion` / `ppq`；MIDI ledger 仍验证通道与音色策略。

### 3.5 InstrumentRole / AssetSlot / AssetBinding

**已验证**

- `InstrumentRole`：melodic / bass / harmony / percussion / texture，支持名称往返与非法值报错。
- `AssetSlot`：名称、类型（instrument/sample）、偏好资产。
- `AssetBinding`：槽位、assetId、provider；`isResolved()` 允许“等待人工选择”的未解析状态。
- Lowering 为每轨生成 `main` 槽与 `symbolic` provider 绑定；`validate-assets` 检查重复槽位、未知槽位、重复绑定。
- Suite Demo 使用 Melodic / Harmony / Bass / Percussion 四种角色，MIDI 后端据此分配通道与 Program。

**设计假设**

- `provider="symbolic"` 表示当前仅有符号音源名；未来 SoundBoxer 绑定使用真实 provider/assetId。
- 每轨当前只有一个 `main` 槽；多槽叠层是接口允许但尚未使用的能力。

### 3.6 表达力验证

**已验证**

| 能力 | 结果 | 测试 |
| --- | --- | --- |
| 跨小节切分 | 音符可跨 barline，offset/duration 精确 | `CrossBarNotesAndTrailingRestsAreRepresentable` |
| 三连音 | `1/12` 全音符在 PPQ 480 下精确为 160 tick | `TripletsUseExactRationalTime`、`MidiTicksFollowPpqAndMusicalTime` |
| 复合拍号 | 6/8、7/8 的小节长度与位置精确；MIDI 时间签名正确 | `CompoundMeterKeepsExactBarPositions`、MIDI meter meta 测试 |
| 多声部 | `parallel` 叠加与多轨道均可表达 | `ParallelOverlaysParts`、Suite Demo |
| 尾部休止 | 显式 span 大于内容末端；reverse 会镜像休止 | `CrossBarNotesAndTrailingRestsAreRepresentable` |

**限制**

- 没有声部/谱表对象，多声部靠并行片段或多轨表达。
- 没有连音线、演奏法、力度曲线与表情。

### 3.7 三个实验算子

实现位置：`include/mpl/music/Harmony.hpp`、`src/music/Harmony.cpp`；IR 节点只声明参数与上下文，不内嵌算法。

- **DiatonicTranspose**：按拼写音级移动，保留半音变化；F minor 下 `F4+2→Ab4`、`B3+1→C#4`。
- **Harmonize**：按音级偏移（默认三度/五度 `{2,4}`）生成和弦音；原音保留 origin，生成音 origin 指向 harmonize 节点。
- **Syncopate**：把落在拍点的音后移指定时值；非拍点不变；span 自动扩展。

三者均有参数校验、错误码、JSON 参数序列化和黄金/结构测试。**非目标**：和声功能、声部进行、风格规则、生成质量。

### 3.8 综合音乐案例

`buildSuiteDemoProject()`：

- 160 BPM、4/4、F minor。
- 段落：Intro（4 小节）、Drop A（8）、Drop B（8）、Outro（4），共 24 小节 = 36 秒。
- 5 轨：Piano（motivic development）、Pad（harmonize 和声）、Bass（roots 低八度）、Lead（syncopate + repeat）、Drums（节奏型 + repeat）。
- 11 个命名 clip；416 音符；21 个 Music IR 对象。
- 和声依赖：Pad/Bass 共享 `i-VI-III-VII` 进行；Pad 由 harmonize 生成三和弦。
- 动机发展：A、音级 +1、音级 +2、reverse、syncopate。
- 产物：`suite.mid`（6 个 MIDI track chunk、480 PPQ、结构校验通过）、Music IR、Production IR、Trace。

### 3.9 Trace 溯源

**已验证**

- Trace 版本 2 包含：
  - `context`（key/meter）；
  - `music` 统计（对象数、求值数、缓存命中）；
  - `objects`（id、kind、label、inputs、contextDependencies、noteCount）；
  - `provenance`（track、clip、source、derivation、逐音符 origin/originLabel/绝对位置）；
  - passes 与 diagnostics。
- 示例：Pad 的 F2 音 origin=`i`（字面量），Ab2/C3 origin=`Pad Voicing`（harmonize 节点）。
- 局限：origin 只记录直接引入者；完整路径由 clip 的 `derivation` 链路表达。没有逐 tick/逐 MIDI 事件的溯源。

### 3.10 文档、ADR 与测试

新增/更新：

- `docs/language-spec.md`
- `docs/semantic-model.md`
- `docs/operator-spec.md`
- `docs/design-decisions/ADR-0001…0005`
- 本报告
- 测试从 41 增至 61 项；黄金文件 4 个。

---

## 4. 修改清单

### 新增

- `include/mpl/music/Key.hpp` + `src/music/Key.cpp`
- `include/mpl/music/Harmony.hpp` + `src/music/Harmony.cpp`
- `include/mpl/ir/Assets.hpp` + `src/ir/Assets.cpp`
- `tests/test_semantics.cpp`
- `tests/golden/suite-demo.mid.hex`
- `docs/` 下规范、语义模型、算子规范、5 个 ADR、本报告

### 修改

- `src/core/Rational.cpp`：受检算术、INT64_MIN 拒绝。
- `include/mpl/music/Fragment.hpp`：`Note::origin`；`Motif`/`Phrase` 别名。
- `include/mpl/ir/MusicIR.hpp` / `src/ir/MusicIR.cpp`：上下文、三个新节点、key/meter 失效、求值统计、origin、JSON v2。
- `include/mpl/ir/ProductionIR.hpp` / `src/ir/ProductionIR.cpp`：删除 MIDI 字段，加入 role/slots/bindings，JSON v2。
- `include/mpl/Project.hpp` / `src/Project.cpp`：Key/Meter 委托、`ClipSpec`、多 clip、角色。
- `src/Compiler.cpp`：资产绑定、多 clip lowering、validate-assets、Trace v2 与 provenance。
- `include/mpl/backend/MidiBackend.hpp` / `src/backend/MidiBackend.cpp`：通道/Program 策略内置、`isExactAtPpq`、角色驱动 percussion。
- `src/examples/CoreDemo.cpp`：时值修正、Suite 案例。
- `tools/mplc/main.cpp`：`core-demo` / `suite-demo`，版本 0.0.2。
- 测试与黄金输出、`CMakeLists.txt`。

---

## 5. 测试与产物

命令：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
.\build\Release\mpl_tests.exe
ctest --test-dir build -C Release --output-on-failure
.\build\Release\mplc.exe core-demo --output build\v002
.\build\Release\mplc.exe suite-demo --output build\v002
```

结果：

- 61/61 测试通过（Debug 与 Release）。
- CTest 1/1 通过。
- 黄金输出：Core Demo Music IR / Production IR / MIDI hex，Suite Demo MIDI hex。
- Suite MIDI 结构校验：格式 1、480 PPQ、6 个 track chunk、全部以 End-of-Track 结束。

指标：

| 案例 | IR 对象 | 轨道 | Clip | 音符 | 时长 | 产物 |
| --- | --- | --- | --- | --- | --- | --- |
| Core Demo | 5 | 2 | 2 | 40 | 4 小节 / 6 秒 | `demo.*` |
| Suite Demo | 21 | 5 | 11 | 416 | 24 小节 / 36 秒 | `suite.*` |

---

## 6. 已知局限

1. 没有文本语言前端；`mplc` 仅提供两个内置案例命令。
2. 没有音频渲染；WAV 需要 SoundBoxer 后端（Phase 5）。
3. Production IR 的 clip 是音符快照，跨层增量编译尚未实现（当前每次编译重新 lowering）。
4. 上下文只有 Key / Meter；调号未写入 MIDI meta，速度/拍号变化映射也仅单事件。
5. `Rational` 比较使用 long double；极端数值不是严格精确比较。
6. MIDI 后端把所有 percussion 轨道放在通道 10，未做鼓组音符映射。
7. 和声、切分算子是最小语义，不建模和声功能、声部进行、风格或张力。
8. Trace 提供音符级 origin 与 clip 级 derivation，不提供逐 tick/逐 MIDI 事件的审计。
9. 一个轨道内多个 clip 可以重叠，当前未视为错误。
10. 段落是命名 clip，不是独立 Section IR 对象。
11. 没有混音/母带、生成算子、约束求解或 AI 模块。

---

## 7. Phase 1 前端建议

1. **最小语法范围**：project / asset / motif / let / pattern / section / track / arrangement；暂不实现 mix/master。
2. **单位安全类型**：`Duration`、`Position`、`Interval`、`Gain`、`Frequency`；字面量后缀 `bpm`、`bars`、`st`、`Hz`、`dB`、`ms`，非法组合在语义分析期报错。
3. **降低到现有 Core API**：Parser/AST 只负责语法与单位，Music IR 构造继续复用 `Project` / `MusicIR`，避免第二套运行时。
4. **诊断带源码位置**：错误码与 Core 对齐（`pitch`、`meter`、`harmony`、`rational` 等），支持多诊断。
5. **黄金策略**：AST 快照 + Music IR JSON + Production IR JSON + MIDI hex；先用 Core 构造等价图作为语义 oracle。
6. **控制范围**：Phase 1 只编译确定性算子与三个实验算子；生成性算子、约束求解、自然语言入口留到后续。
7. **先做语义后做语法糖**：任何语法特性都必须能映射到现有 IR 对象或明确的新节点，不新增临时 API。

---

## 8. 验收对照

| 任务 | 状态 | 证据 |
| --- | --- | --- |
| 1 时间系统审查 | 完成 | 6 秒/7680 tick 测试；ADR-0001 |
| 2 Rational 加固 | 完成 | 受检算术；溢出/零分母测试；PPQ 精确性 API |
| 3 DAG/上下文/增量 | 完成 | 上下文失效与增量一致性测试 |
| 4 Production IR 后端无关 | 完成 | 字段删除；JSON 断言；MIDI 策略内移 |
| 5 角色/资产接口 | 完成 | Assets API、validate-assets、Suite 使用 |
| 6 表达力验证 | 完成 | 跨小节、三连音、复合拍号、多声部、尾部休止测试 |
| 7 三个实验算子 | 完成 | Harmony API、IR 节点、上下文依赖、溯源测试 |
| 8 综合案例 | 完成 | Suite Demo：24 小节、5 轨、11 clip、416 音符 |
| 9 Trace 溯源 | 完成 | Trace v2：objects/derivation/origin |
| 10 文档/ADR/测试 | 完成 | 规范、语义模型、算子规范、5 ADR、61 测试 |
