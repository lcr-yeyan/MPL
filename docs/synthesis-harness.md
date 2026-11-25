# Model-Agnostic Synthesis Harness

状态：V0.4.0-alpha 基础设施

代码：`include/mpl/lab/Harness.hpp`、`src/lab/Harness.cpp`、`tools/mpl_lab`

## 1. 定位

Harness 独立于编译器核心与模型供应商。它只负责：

1. 读取 experiment JSON；
2. 读取外部提交的候选程序；
3. 编译并执行任务验证；
4. 记录结构化状态、Token/调用/修复与耗时；
5. 输出统一结果 JSON。

Harness **不调用任何模型 API**。缺少 API 时，外部可导入模型输出并跑完整流程；没有导入时
结果为 `not-run`，不报告成功率。

## 2. Experiment Schema

```json
{
  "id": "offline-factorial-example",
  "task": "../tasks/01-basic-notes.json",
  "submissions": [
    {
      "arm": "C1",
      "candidate": "../baselines/basic.mpl",
      "initialCandidate": "../baselines/basic.mpl",
      "inputTokens": 0,
      "outputTokens": 0,
      "cacheTokens": 0,
      "modelCalls": 0,
      "repairs": 0
    }
  ]
}
```

| 字段 | 语义 |
| --- | --- |
| `id` | 实验标识 |
| `task` | 任务 JSON 路径，相对 experiment 文件解析 |
| `submissions[].arm` | A0/A1/B0/B1/C0/C1 或外部标签 |
| `candidate` / `initialCandidate` | 最终/首次候选程序路径 |
| `audio` / `renderManifest` | 可选 WAV 与可信 manifest；两者同时匹配才用于音频观察 |
| `inputTokens` / `outputTokens` / `cacheTokens` | 模型 Token；离线导入为 0 |
| `modelCalls` | 模型调用次数；0 表示非模型生成 |
| `repairs` | 实际修复轮数 |

Schema 文件：`bench/experiment-schema.json`。

## 3. 结果

```json
{
  "format": "mpl.experiment-result",
  "version": 2,
  "experimentId": "...",
  "taskId": "...",
  "modelGenerated": false,
  "totals": {
    "runs": 6,
    "inputTokens": 0,
    "outputTokens": 0,
    "cacheTokens": 0,
    "modelCalls": 0,
    "passed": 6,
    "completed": 6
  },
  "observedSuccessRate": null,
  "runs": [
    {
      "arm": "B0",
      "feedback": false,
      "candidate": "...",
      "status": "pass",
      "initialStatus": "",
      "elapsedMilliseconds": 12,
      "latencyMilliseconds": 12.0,
      "candidateBytes": 512,
      "inputTokens": 0,
      "outputTokens": 0,
      "cacheTokens": 0,
      "modelCalls": 0,
      "repairs": 0,
      "model": "",
      "modelVersion": "",
      "sampling": "",
      "promptVersion": "",
      "error": "",
      "verification": { "...": "..." }
    }
  ]
}
```

规则：

- `modelGenerated` 只有至少一次 `modelCalls > 0` 才为 true；
- `observedSuccessRate` 在非模型生成或没有完成运行时为 `null`；
- 所有失败原因、编译诊断与比较结果都进入 `runs`；
- 不把 `unsupported` / `not-run` 计入共同成功率。

## 4. Arms

| Arm | 表示 | Harness 行为 |
| --- | --- | --- |
| A0 | Flat Event JSON | 解析 Flat 事件，使用共同观察与验证器 |
| A1 | Flat Event JSON + Semantic Feedback | 同上；记录 feedback 标记 |
| B0 | MPL Core | 编译 + 验证；拒绝直接或间接 stdlib 导入 |
| B1 | MPL Core + Semantic Feedback | 同上；记录 feedback 标记 |
| C0 | MPL + Standard Library | 编译 + 验证 |
| C1 | MPL + Standard Library + Semantic Feedback | 同上；记录 feedback 标记 |

- Flat 不可表达的任务（和声时间线、模块依赖、frame 等）返回 `not-applicable`，不进入共同成功率；
- B0/B1 若模块闭包含 `Std*`，返回 `invalid-arm`；
- A1/B1/C1 的修复循环由外部控制；Harness 记录 `feedback=true` 与 repairs，不自行调用模型。

## 5. 修复循环

Harness 只接收已产生的候选与 `repairs` 计数，不自行调用模型。外部循环可：

1. 生成候选；
2. 运行 `mpl_lab verify`；
3. 把结构化诊断反馈给模型；
4. 再提交新候选，`repairs` 递增；
5. 最终用 experiment 记录全部尝试。

V0.4.0-beta 不实现自动反馈循环。

## 6. Token 与模型元数据

记录：

- `inputTokens`：提示词、语言规范、标准库上下文、诊断反馈等全部输入；
- `outputTokens`：程序、解释与修复输出；
- `cacheTokens`（若供应商提供）；
- `modelCalls`；
- `latencyMilliseconds`；
- `model` / `modelVersion` / `sampling` / `promptVersion`；
- `error`；
- `candidateBytes`（仅程序长度，不等于 token）。

没有模型调用时这些字段为 0，成功率 `null`。随机种子不支持时由外部记录为 `"unsupported"`，不伪造。

## 7. 模型适配器接口

`include/mpl/lab/ModelAdapter.hpp` 定义可替换接口：

```cpp
class ModelAdapter {
    virtual std::string name() const = 0;
    virtual ModelResponse generate(const ModelRequest&) = 0;
};
```

- 请求包含 task、prompt、语言/标准库上下文、上次候选、语义反馈、attempt、预算、采样配置；
- 响应包含候选源、模型/版本、采样/提示词版本、输入/输出/缓存 Token、调用次数、延迟、错误；
- Adapter 不得进入 MPL Core/Runtime；
- 供应商不支持随机种子时，在 `sampling` 中记录 `"seed":"unsupported"`，不伪造。

V0.4.0-beta 未实现任何供应商 adapter，也未调用模型 API。

## 8. 离线示例

`bench/experiments/factorial-basic.json` 用同一任务运行 A0/A1/B0/B1/C0/C1；Flat 与 MPL
候选都通过，`modelGenerated=false`、`observedSuccessRate=null`。这是基础设施验证，
不是模型实验。

`bench/experiments/offline-example.json` 使用带 frame 的 local-edit 任务；Flat 组返回
not-applicable，用于验证“不适用任务不混入成功率”。

## 9. 范围限制

- 不实现模型供应商适配器；
- 不实现自动 prompt/repair 循环；
- 不实现并发/队列；
- 不实现完整 A/B/C/D 任务调度器；
- 不伪造 Token 或成功率。
