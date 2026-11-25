# ADR-0034：按任务的上下文构建

状态：已采纳（V0.5.0-alpha）

## 背景

完整 LCP 对简单任务过大；但减小上下文不能静默删除必需语义，也不能用 held-out 参考解
填充。

## 决策

1. 三种模式：Full / Domain / Adaptive。
2. Full = 完整 LCP + 全部知识；Domain = L1 基础 + domain 知识；Adaptive = L1 + 任务相关知识。
3. 预算不足返回明确状态；Adaptive 可 fallback Full，但不得静默截断必需内容。
4. Context 记录 hash、bytes、估算 Token、选择/排除原因、依赖、sha256、耗时。
5. Task Contract 只包含 observation/frame 名称，不含 expected 值。
6. held-out 泄漏扫描；发现 expected 值则阻断。
7. 去重按行；不声称语义最小或全局最优。

## 结果

离线 10 任务：Full 4,898 est tokens、Domain 1,426、Adaptive 1,952；Domain 覆盖 1.0，
Adaptive 覆盖 0.6。Domain 在当前集合优于 Adaptive，需在 beta 重新设计相关性/覆盖。

## 后果

- 上下文可复现、可审计；
- 缩减不代表模型成功率不变；
- Adaptive 不是默认最优；
- 真实模型比较必须单独授权/执行。
