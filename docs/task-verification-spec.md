# Task Verification Specification（V1）

状态：V0.4.0-beta 实现规范

代码：`include/mpl/lab/Task.hpp`、`src/lab/Task.cpp`、`tools/mpl_lab`

## 1. 目标

验证器独立于生成模型与 MPL 源语言语法。它只读取：

- 任务 JSON；
- 基线 MPL 程序；
- 候选 MPL 程序；
- 可选渲染 WAV。

生成器“声称完成”不构成通过。通过条件必须由观察比较得出。

## 2. 任务 JSON

```json
{
  "id": "mpl-bench-001-basic-notes",
  "title": "Basic notes and quarter-note rhythm",
  "category": "basic_notes_rhythm",
  "baseline": "../baselines/basic.mpl",
  "allowedScope": "program",
  "tolerance": 0.0,
  "budget": { "maxRepairs": 3, "maxSeconds": 120, "maxTokens": 8000 },
  "target": [
    { "name": "event_count", "value": 4 },
    { "name": "onset_set", "value": ["0", "1/4", "1/2", "3/4"] }
  ],
  "preserve": [
    { "name": "logical_extent" }
  ]
}
```

字段：

| 字段 | 必需 | 语义 |
| --- | --- | --- |
| `id` | 是 | 全局任务 ID |
| `baseline` | 是 | 相对任务文件解析的 MPL 路径 |
| `allowedScope` | 否 | `program`（允许任意程序改动）或 `none`（必须与 baseline 源相同） |
| `tolerance` | 否 | 全局数值容差 |
| `budget` | 否 | `maxRepairs` / `maxSeconds` / `maxTokens`，供 harness 记录与约束 |
| `target` | 是 | 必须满足的观察 |
| `preserve` | 否 | 必须保持 baseline 值的观察 |

目标条目：

```json
{"name":"event_count","value":4}
{"name":"normalized_rhythm","fromBaseline":true}
{"name":"sample_peak","value":-6.0,"tolerance":0.5}
```

- `value`：显式期望值（任意 JSON）；
- `fromBaseline`：以 baseline 观察为期望；
- `tolerance`：该条目的数值容差，取 max(任务容差, 条目容差)。

## 3. 验证算法

1. 解析任务；未知观察名、缺少 target、非法 scope/frame 直接 `error`。
2. 编译 baseline；失败 `error`。
3. 编译候选；失败 `error`，并记录结构化诊断。
4. `allowedScope=none`：候选源与 baseline 源必须逐字节相同。
5. 计算 baseline/candidate 观察。
6. 对每个 target：期望来自 `value` 或 baseline；候选缺失/不可验证/比较失败分别记录。
7. 对每个 preserve：期望固定为 baseline 值。
8. 若声明 `frame`：计算 Semantic Delta，逐对象/属性检查 allow/deny；未授权变化失败，
   并记录 before/after/source/cause。
9. 状态：
   - 任一 fail → `fail`；
   - 否则任一 unverifiable → `unverifiable`；
   - 否则 `pass`。
   - 契约分别记录：`validityStatus` / `targetStatus` / `preserveStatus` / `frameStatus`。

编译失败优先于观察比较；`unverifiable` 永不报告为 `pass`。
Flat Event 候选对不可表达观察或 frame 返回 `not-applicable`，不进入共同成功率。

## 4. Allowed Scope 与 Frame

V0 支持两种 `allowedScope`：

- `program`：候选可修改程序任意部分；
- `none`：候选必须与 baseline 完全相同（用于“纯验证”任务）。

V1 增加可执行 `frame`：

```json
"frame": {
  "allow": [{"object":"clip","key":"Keys/Keys#0","property":"symbolicEvents"}],
  "deny": [{"object":"production","key":"*","property":"tempo"}]
}
```

- `allow` 为空表示不允许任何变化；
- `deny` 优先；
- 未授权变化进入 comparisons，带 before/after/source/cause；
- 详细规范：`docs/frame-contract-spec.md`。

## 5. 资源预算

`budget` 由 harness 记录并在生成/修复循环中执行；验证器本身不消耗模型 token。
`maxRepairs`、`maxSeconds`、`maxTokens` 是上限，不是成功条件。

## 6. 状态与 JSON

```json
{
  "format": "mpl.task-verification",
  "version": 1,
  "taskId": "...",
  "status": "pass|fail|unverifiable|error|not-applicable",
  "validityStatus": "ok|failed",
  "targetStatus": "pass|fail|unverifiable",
  "preserveStatus": "pass|fail|unverifiable",
  "frameStatus": "pass|failed|restricted|unrestricted|not-applicable",
  "comparisons": [
    {"name":"event_count","status":"pass","expected":"4","actual":"4","reason":""}
  ],
  "diagnostics": []
}
```

## 7. 音频任务

`sample_peak` / `true_peak` / `integrated_loudness` 需要 WAV：

```text
mpl_lab verify task.json candidate.mpl --audio candidate.wav
```

未提供 WAV 时这些目标为 `unverifiable`，任务不会因此 `pass`。

## 8. 独立性

- 验证器不读取模型提示词、对话或生成记录；
- 不信任候选程序自报的观察值；
- 不调用生成模型；
- 只依赖编译器、观察注册表与可选 WAV。

## 9. 测试

`tests/test_lab.cpp` 覆盖：

- JSON 解析；
- baseline 通过；
- 未修改候选在编辑任务上失败；
- preserve 违反失败；
- 无 WAV 的 mastering 任务返回 unverifiable；
- 未知观察解析错误。
