# Context Accessibility 001（V0.5.0-alpha）

## 1. 实验

完全离线，无模型调用。10 个任务、同一知识库、同一 LCP v1.1、预算 3000 estimated tokens。

模式：

- F：Full（完整 LCP + 全部 33 知识单元）
- D：Domain（L1 基础 + 任务 domain 知识 + 依赖）
- A：Adaptive（L1 基础 + 任务相关知识与依赖，maxKnowledge=8）

原始结果：`bench/results/context-accessibility-001/context-accessibility.json`。

## 2. 汇总

| 模式 | 平均 bytes | 平均 estimated tokens | 所需知识覆盖 | 预算溢出 | 重复行 | 无关知识 | 平均构建 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Full | 19,590 | 4,898 | 1.00 | 10/10 | 99 | 27 | 5.0 ms |
| Domain | 5,701 | 1,426 | 1.00 | 0/10 | 13 | 1 | 1.2 ms |
| Adaptive | 7,806 | 1,952 | 0.60 | 0/10 | 19 | 9 | 2.0 ms |

- Full 在 3000 token 预算下全部溢出；没有静默删除，报告 `insufficient-budget`。
- Domain 相对 Full 减少约 **70.9% bytes / 70.9% estimated tokens**，并保持 domain 知识覆盖 1.0。
- Adaptive 相对 Full 减少约 **60.2% bytes**，但平均所需知识覆盖仅 0.60；它并没有在该任务集上优于 Domain。
- 所有模式 contextHash 在重复构建中一致；`deterministic=true` 10/10。
- 重复行来自 LCP 内部与知识片段的共享行；去重后仍保留 13–99 行重复（按行去重只移除完全相同行）。

## 3. 解释

- Domain 更便宜且覆盖更完整，说明“按域选择”在该任务集上是更强基线；
- Adaptive 的知识 cap 与相关度过滤导致必要领域知识缺失（coverage 0.6），不是“更聪明”；
- Full 的 33 单元 + 完整 LCP 超过预算，说明完整上下文不可作为所有任务的默认；
- 上下文缩减**不代表**真实模型成功率保持不变；本轮没有模型调用。

## 4. 确定性

逐任务重复构建两次，Context hash 全部一致。构建时间依赖机器，仅作参考。

## 5. 限制

- Token 为 `ceil(bytes/4)`，不是 tokenizer 精确值；
- 任务数 10，不代表完整任务分布；
- Domain 上下文包含整个 domain，可能仍含无关知识；
- 未做真实模型上下文效果比较；
- 没有 held-out 任务参与本实验。
