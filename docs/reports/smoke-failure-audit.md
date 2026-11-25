# Smoke Failure Audit（V0.4.0-rc）

日期：2026-11-25

所有原始输出保存在 `bench/results/smoke-001/` 与
`bench/results/factorial-smoke-001/`。

## 1. Local-edit smoke（1 call）

- 请求：修改 baseline 最后一个 F4 为 G4；
- 模型：`deepseek-flash`（请求 `deepseek-chat`）；
- 240 prompt + 158 completion = 398 tokens；1,608 ms；
- 结果：**pass**（target/preserve/frame/validity）。
- 无失败。

## 2. Factorial smoke task 01（6 calls + 1 repair）

| Arm | 结果 | 首个诊断 | 原因 |
| --- | --- | --- | --- |
| A0 | pass | — | Flat Event JSON 正确 |
| A1 | pass | — | 同上 |
| B0 | error | `parse-expected-lbrace` / `parse-expected-equal` | 模型输出伪 MPL：`project "Basic"`、`tempo 120`、无 `;` |
| B1 | error | 同上 | 同上 |
| C0 | error | 同上 | 同上 |
| C1 | error | 同上 | 同上 |
| C1 repair | error | `parse-expected-equal` / `parse-missing-play` | 修复后仍有非法 project/track/clip 语法 |

### 级联诊断

首个 `project "Basic"` 解析错误后，后续 `parse-unexpected-project-field` 多为级联；
不应把每个级联都当作独立语言缺陷。

### 语言规范对应

模型输出使用了不存在的语法：

- `project "Basic" { ... }`（project 名不能加引号）
- `tempo 120 bpm`（缺 `=` 和 `;`）
- `asset ...` 放在 project 内（应在顶层）
- `track "Keys" role melodic { ... }`（track 语法错误）
- `clip Keys { notes { ... } }`（clip 必须是 `play`）
- `section S { bars 1 }`（section 需要 `S(1 bars)`）
- 缺 `arrangement` / `export`

### 上下文缺失/示例不一致

- L0-L2 上下文在首次 smoke 中不存在或未使用；factorial smoke 使用的是短任务提示，未提供完整语法。
- 这不是“语言无法表达”，而是模型未获得足够语法约束，且任务提示没有 MPL 代码样例。
- LCP v1.1 增加明确语法/反例后，Learnability L1 2/6、L2 5/6，说明上下文有效但不是充分条件。

### 修复失败原因

C1 反馈只包含编译诊断；模型仍未修正 `project`/`tempo`/`play` 结构。修复不是确定性的，
不能把一次修复失败归因于单一语法点。

## 3. Learnability 失败审计

### Voice-leading 列表索引

- 模型写 `pitches[0]`；
- MPL 不支持列表索引，首个错误 `parse-expected-rparen`；
- LCP v1 没有明确禁止；这是**上下文缺失**。
- v1.1 增加禁止说明和 `cycle(R, voicingNotes(V), velocity)` 示例后，L2+ 通过。

### Local-edit frame 违反

- 目标音高、事件数、起音、长度均正确；
- 模型把 `track Piano` 改为 `track Keys`（示例名称污染）；
- Frame Contract 报告 `frame:track.track` 与 `frame:production.mix` 失败；
- 这不是 `pitch_sequence` 目标失败，而是作用域越权。
- 明确写出“track Piano 不变，不要复制示例名称”后，单次 B0 调用 pass。

### L0 语法失败

- L0 仅输出协议，5/6 `parse-*`；
- 这是最小上下文不足，不是核心语言缺陷。

## 4. 结论

- 没有任何证据要求修改 Language 0.3.0 核心语法；
- 失败主要来自上下文不足、示例名称污染、模型幻觉（列表索引/伪语法）；
- 需要的是 LCP/提示词/验证反馈改进，不是放宽编译器；
- 所有失败候选和诊断已保留，可复现。
