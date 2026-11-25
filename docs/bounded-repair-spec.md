# Bounded Repair Specification（V0.4.0-rc）

代码：`include/mpl/lab/Repair.hpp`、`src/lab/Repair.cpp`

## 1. 循环

```text
runRepairLoop(adapter, task, taskPath, initialResponse, budget, redactExpected)
```

1. 接收 initial response；
2. 编译并独立验证；
3. 未通过则构造反馈；
4. 调用 adapter 生成下一候选；
5. 预算内重复；
6. 保留每轮候选与验证证据。

## 2. 反馈内容

- structured compiler diagnostics；
- failed observation names/status/reason；
- frame/validity/target/preserve 状态；
- Semantic Delta 摘要（对象/属性/cause/identity）；
- 不包含 held-out expected 值（`redactExpected=true` 时 expected 字段为 `<redacted>`，delta 去掉 before/after）。

## 3. 禁止项

- 不修改 task 的 target/preserve/frame；
- 不放宽 tolerance；
- 不把 not-applicable/unverifiable 改为 pass；
- 不向模型暴露 held-out 参考候选/音频/参考解。

## 4. 预算

`RepairBudget`：

- `maxAttempts`；
- `maxInputTokens` / `maxOutputTokens`；
- `maxSeconds`。

任一超限 → `BudgetExhausted`，保留已生成 attempts。

## 5. 失败分类

| 值 | 条件 |
| --- | --- |
| `model-failure` | adapter 返回 ok=false / 空候选 / provider error |
| `compile-failure` | 候选编译失败 |
| `task-failure` | 编译通过但 target/preserve/frame 未达 |
| `infrastructure-error` | 文件写入/读取、验证器内部错误 |
| `budget-exhausted` | 尝试/Token/时间预算耗尽 |

分类独立于成功；不能把预算耗尽说成模型失败或任务失败。

## 6. 证据

`RepairResult::attempts[]` 保存：

- 候选源；
- 源文件路径；
- 验证结果（target/preserve/frame/validity）；
- 反馈；
- 每轮 Token/调用。

`toJson()` 输出完整记录；实验运行器可将其写入 raw results。

## 7. 当前实现边界

- Adapter 由调用方注入；没有供应商默认值；
- 不自动构造 prompt 的完整语言规范（由实验配置提供）；
- 不自动重试 provider 网络错误；
- 每轮候选写入工作目录 `.mpl-repair-N.mpl`，作为证据文件。
