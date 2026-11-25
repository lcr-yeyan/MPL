# Statistical Reporting Specification（V0.4.0 Final）

代码：`include/mpl/lab/Statistics.hpp`、`mpl_lab stats`

## 1. 原始结果

每个 experiment/file 记录：

- experimentId / taskId / arm / feedback；
- initialStatus / finalStatus；
- validity / target / preserve / frame；
- syntax/type/compilation 失败分类；
- input/output/cache tokens；
- modelCalls / latency / repairs / provider model；
- error/timeout/budget 状态。

Raw results 仅在真实执行后生成。

## 2. 指标

| 指标 | 定义 |
| --- | --- |
| First-attempt success | 第一次候选通过全部契约 |
| Final trial success | 有界修复后的最终候选通过 |
| Syntax validity | 不出现 `parse-*` 错误 |
| Type validity | 语法有效且无 `type-*`/`sema-*` 错误 |
| Conditional semantic success | 类型有效候选中满足 target 的比例 |
| Target satisfaction | targetStatus=pass |
| Preserve violation | preserveStatus=fail |
| Frame violation | frameStatus=failed |
| Tokens per trial | 该 trial 全部输入+输出+缓存 |
| Repair count | trial 内第二次及以后调用数 |
| Task-clustered CI | 以任务为 cluster 的配对 bootstrap |

## 3. 分类

- `pass`：通过；
- `fail`：编译通过但 target/preserve/frame 失败；
- `unverifiable`：观察不可验证；
- `not-applicable`：表示能力不适用；
- `invalid-arm`：B 组使用 stdlib；
- `model-error`：API 错误/超时/空响应；
- `budget-exhausted`：预算耗尽。

API error、timeout、budget 不得计为 task fail 或 success；按预注册规则单列并排除/敏感性分析。

## 4. 配对与 CI

- 同一任务实例跨 arm 配对；
- 差异使用确定性 bootstrap（2000 次，seed=1）95% CI；
- 同一任务重复生成按实例分别记录；
- n=5 的 pilot 只作探索，不作确认性结论。

## 5. 当前结果

- Learnability v1/v1.1：见 `docs/reports/learnability-001.md`；
- Pilot：30/30 pass、77,418 tokens、41,171 ms；因精确 target 暴露，存在 ceiling；
- 配对差异全为 0，95% CI [0,0]；
- held-out 未执行。

统计报告必须同时说明样本量、任务数、模型实际版本、是否模型生成、是否有 ceiling，
不得把 30/30 解读为语言表示优势。
