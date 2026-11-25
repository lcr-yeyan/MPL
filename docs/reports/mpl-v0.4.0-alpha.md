# MPL V0.4.0-alpha 开发报告

日期：2026-11-25

## 1. 结论

V0.3.0 已完成 Final 语义审计并冻结；V0.4.0-alpha 的智能音乐程序合成实验基础设施已建立：

- 机器可读编译器报告；
- Observation Registry V0（16 项）；
- 独立任务验证器（JSON task）；
- MPL-Bench-001（12 类任务）；
- Model-Agnostic Synthesis Harness（离线导入模式）；
- 实验 Schema 与基线/回归报告。

**没有调用任何模型 API。** 因此本报告只声明基础设施完成情况，不报告模型生成成功率，
不虚构 Token 或实验数据。

## 2. Phase A：V0.3.0 Final Audit

审计报告：`docs/reports/v0.3.0-final-architecture-audit.md`

结论：通过，V0.3.0 冻结。

实际修复：

1. **孤立 continuation**：RC 可构造没有前驱 sounding event 的 continuation，combine 会
   静默丢弃。Final 增加 `rhythm-continuation-orphan` 验证。
2. **模块传递身份**：RC 的 contentHash 不含依赖。Final 增加 `resolvedHash`，依赖变化会
   改变模块闭包身份。
3. **voiceLead 状态语义**：预算耗尽时保留已找到候选但明确 `searchComplete=false`；文档
   不再暗示“全局最优”。
4. **MRP extent 复核**：logical/content/processor-tail/render extent 分离确认；
   921,818 帧解释清晰。

新增针对性回归 18 项；RC 的 277 项全部保留；合计 **295/295**（Debug/Release）。

## 3. Phase B：Machine-Readable Compiler Interface

### 3.1 C++ API

`Compilation::toReportJson()`：

```json
{
  "format": "mpl.compiler-report",
  "version": 1,
  "compilerVersion": "0.3.0",
  "languageVersion": "0.3.0",
  "stdlibVersion": "0.3.0",
  "stdlibHash": "...",
  "phases": {
    "compile": "ok",
    "validate": "ok",
    "execute": "not-run",
    "backend": { "status": "not-run", "approximations": [] }
  },
  "diagnostics": [],
  "passes": [],
  "modules": [
    {"name":"StdHarmony","contentHash":"...","resolvedHash":"..."}
  ],
  "counts": {"musicObjects":144,"tracks":4,"clips":14,"notes":247}
}
```

- `stdlibHash` 由 `Std*` 模块 resolvedHash 组合；
- 模块闭包身份包含传递依赖；
- 编译/验证状态独立；执行/后端在编译器阶段为 `not-run`；
- 人工 CLI 输出保持不变。

### 3.2 CLI

```text
mplc report <file.mpl>
mplc check <file.mpl> --json
mplc analyze <file.wav> --json
```

- `mplc report` 成功输出 compiler report；前端失败时输出带 `diagnostics` 的同一格式
  wrapper，诊断本身保留 SourceSpan、expected/actual、related 与稳定 code；
- `mplc check --json` 继续输出前端诊断数组，兼容既有机器消费者。

### 3.3 诊断

前端 `DiagnosticSink::toJson()` 已提供：

- severity / code / message；
- SourceSpan（file/line/column/offset/length）；
- expected / actual / related。

编译器阶段 `Diagnostic` 提供 code/severity/message；后端能力/近似通过 CLI 与 audio plan
诊断输出。没有把后端未执行误报为通过。

## 4. Phase C：Observation Registry V0

规范：`docs/observation-registry-spec.md`

实现：`include/mpl/observe/Observations.hpp`

16 项观察覆盖：

- 结构：logical/content extent、event count、onset set/density；
- 音高/节奏：pitch/interval sequence、normalized rhythm、fragment range；
- 和声：harmonic timeline；
- 模块：module dependencies；
- 轨道：symbolic events；
- 音频：sample peak、true peak、integrated loudness；
- 声部：voice movement（V0 明确不可从工程自动推导）。

每项声明输入类型、作用域、单位、计算语义、适用条件与 exact/tolerance/unverifiable 分类。

## 5. Phase D：External Task Verification

规范：`docs/task-verification-spec.md`

实现：`include/mpl/lab/Task.hpp`、`tools/mpl_lab`

- 任务 JSON：id/baseline/target/preserve/allowedScope/tolerance/budget；
- 验证器独立编译 baseline 与 candidate；
- 不信任生成器声明；
- 支持显式值、`fromBaseline`、条目容差；
- 状态：pass / fail / unverifiable / error；
- `allowedScope` 仅支持 `program` / `none`，其他值解析即报错；
- 音频目标无 WAV 时为 unverifiable。

## 6. Phase E：MPL-Bench-001

目录：`bench/`

12 类任务：

1. 基础音符与节奏；
2. 不对称拍号；
3. 多周期关系；
4. 主题变换；
5. 和声驱动生成；
6. 和弦排列；
7. 声部连接；
8. 节奏替换；
9. 和声替换；
10. 局部结构编辑；
11. 混音与自动化；
12. 母带技术目标。

每个任务有 baseline、target、preserve、tolerance、budget 与失败条件。Baseline 自验证：
01–07、11 pass；08–10 故意要求改动，baseline fail；12 无 WAV 时 unverifiable。
`bench/candidates/` 中正确候选在 08–10 pass。

这些是**验证器回归结果**，不是模型成功率。

## 7. Phase F：Model-Agnostic Synthesis Harness

规范：`docs/synthesis-harness.md`

实现：`include/mpl/lab/Harness.hpp`、`tools/mpl_lab run`

- 读取 experiment JSON；
- 支持 A/B/C/D arm；A（直接 MIDI JSON）当前标记 unsupported，不混入共同成功率；
- 编译 + 验证；
- 记录 input/output tokens、modelCalls、repairs、耗时、候选字节数、结构化诊断；
- `modelGenerated=false` 时 `observedSuccessRate=null`；
- 不调用模型，不实现自动修复循环。

离线示例：`bench/experiments/offline-example.json`。

## 8. 实验对照

设计支持：

| Arm | 表示 | 状态 |
| --- | --- | --- |
| A | 直接 MIDI 事件 JSON | Harness 标记 unsupported |
| B | MPL 基础语言 | 可验证 |
| C | MPL + Standard Library | 可验证 |
| D | MPL + Standard Library + Verification Feedback | 可验证；反馈由外部循环 |

未执行模型调用，因此：

- 不报告首次/修复后成功率；
- 不报告模型 Token；
- 不把 A 组混入 MPL 成功率。

## 9. 测试与回归

- 全量：295/295（Debug 与 Release 均通过）；
- RC 277 项全部保留；
- 新增 18 项：Final 审计回归、compiler report、observations、task verifier、harness。

详见 `docs/reports/baseline-and-regression-v0.4.0-alpha.md`。

## 10. 交付物

- V0.3.0 Final（版本 0.3.0，测试 295/295）
- `docs/reports/v0.3.0-final-architecture-audit.md`
- `docs/reports/mpl-v0.4.0-alpha.md`
- `docs/observation-registry-spec.md`
- `docs/task-verification-spec.md`
- `docs/mpl-bench-001.md`
- `bench/experiment-schema.json`
- `docs/synthesis-harness.md`
- `bench/tasks/` 12 任务、`bench/baselines/`、`bench/candidates/`
- `mpl_lab` 工具
- `docs/reports/baseline-and-regression-v0.4.0-alpha.md`

## 11. 范围限制

未实现：情绪/Taste 类型、完整自主 Agent、通用约束求解、完整泛型/递归、完整插件宿主、
SoundBoxer Adapter、大规模 DSP、完整吉他指法、未验证自动近似。

未调用模型：不声称任何模型成功率。

## 12. 下一步

1. 接入真实模型 API 的适配器（保持 Harness 接口不变）；
2. 自动反馈/修复循环；
3. 更多观察（和声功能、频谱、混音目标）；
4. 任务级别的资产与路由；
5. A 组 MIDI JSON 观察适配器。
