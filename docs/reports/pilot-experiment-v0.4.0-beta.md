# Pilot Experiment Report（V0.4.0-beta）

日期：2026-11-25

## 状态：未执行真实模型试运行

当前环境没有可用的模型 API 调用授权与调用预算，因此本阶段**没有**执行 30–60 次模型
试运行。

本报告不包含：

- 模型调用次数；
- 输入/输出/缓存 Token；
- 首次或修复后成功率；
- 延迟统计；
- 任何模型生成程序或艺术质量评价。

以上数据均未产生，不得引用为实验结果。

## 已具备的离线能力

- `bench/experiments/factorial-basic.json`：A0/A1/B0/B1/C0/C1 六组离线候选；
- `bench/experiments/offline-example.json`：带 frame 的 local-edit，Flat 组 not-applicable；
- `mpl_lab run` 记录模型元数据字段、Token、调用、延迟、修复与错误；
- 无模型调用时 `modelGenerated=false`、`observedSuccessRate=null`；
- B0/B1 拒绝 stdlib；
- 独立任务验证器与 Observation Registry 已冻结。

## 离线验证结果（非模型）

| 实验 | 结果 |
| --- | --- |
| factorial-basic A0–C1 | 6/6 pass，modelGenerated=false |
| offline local-edit（A0/A1） | not-applicable（frame） |
| offline local-edit（B0/B1/C0/C1） | pass |

这些结果只说明基础设施能编译、验证与区分适用性，不代表模型能力。

## 下一步（需要授权）

1. 配置模型适配器（不进入 Core/Runtime）；
2. 设置固定提示词/语言上下文版本；
3. 为 dev 任务分配调用预算；
4. 先运行小规模 pilot（30–60），检查：
   - 反馈循环是否有效；
   - Flat/MPL 是否公平；
   - 不适用任务是否被排除；
   - 预算与失败原因是否记录完整；
5. 冻结任务/验证规则后再进入 held-out。

## 结论

V0.4.0-beta 的 pilot 实验基础设施已就绪，但真实 pilot **尚未执行**。本报告不报告模型
成功率或艺术质量。
