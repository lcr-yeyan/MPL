# Adaptive Context Specification（V0.5.0-alpha）

## 1. 模式

| 模式 | 内容 |
| --- | --- |
| Full | 冻结完整 LCP + 全部知识单元 |
| Domain | L1 语法/类型基础 + 指定 domain 的全部知识 + 依赖 + task contract |
| Adaptive | L1 基础 + 任务相关知识与依赖（最多 maxKnowledge）+ task contract |

Full 是基线，不声称最小；Domain/Adaptive 是工程缩减。

## 2. 构建

1. 读取基础 LCP（默认 `lcp/v1.1/contexts/L1.md`；Full 用 Core/C）；
2. 从 Knowledge Resolution 选知识（Adaptive 过滤不相关、cap）；
3. 依赖闭包；
4. 去重（逐行）；
5. 追加 Task Contract（只含 observation/frame 名称，不含 expected 值）；
6. 计算 SHA-256、bytes、`ceil(bytes/4)` 估计 Token；
7. 预算检查；
8. held-out 泄漏检查。

## 3. 预算

- `tokenBudget=0`：无限；
- 预估 Token > budget：`overflow=true`；Adaptive 可 fallback 到 Full；
- 必要基础内容不静默删除；
- 若 Full 也超预算：`budgetStatus=insufficient-budget`。

## 4. 泄漏防护

- Task Contract 不包含 target expected 值；
- held-out 模式扫描 content；若发现长度≥8的 expected 字符串/JSON → `leakage-blocked`；
- 不使用 held-out 参考候选/音频；
- 知识示例不得引用 held-out 答案（库中示例来自 dev）。

## 5. 报告

`mpl.context-build` v1：

- mode/domain/taskId；
- contextHash、bytes、estimatedTokens；
- budgetStatus/overflow/fallbackUsed/leakage；
- duplicateLines、elapsedMilliseconds；
- selected/excluded/dependencies；
- sources（id/kind/mandatory/bytes/sha256）；
- diagnostics。

## 6. 当前对照结果

见 `docs/reports/context-accessibility-001.md`：

- Full 平均 19,590 bytes / 4,898 tokens，10/10 预算溢出；
- Domain 5,701 / 1,426，coverage 1.0；
- Adaptive 7,806 / 1,952，coverage 0.6。

自适应不是全局最小；Domain 在该任务集上更便宜且覆盖更完整。上下文缩减不代表模型成功率不变。

## 7. 限制

- Token 为 bytes/4 估计；
- 去重按行，非语义去重；
- 不保证全局最小；
- 未做真实模型上下文效果对比。
