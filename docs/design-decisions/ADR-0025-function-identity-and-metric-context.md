# ADR-0025：函数身份与度量上下文缓存键

状态：已采纳（V0.3.0-beta）

## 背景

Alpha 的函数缓存键是“函数名 + 版本 + Key + 单一 Meter + 实参”。变拍号引入 Section 局部 MeterMap 与 anchor 后，同一函数在不同 Section 可能产生不同结果；若继续只用单一 Meter，会错误共享缓存。

## 决策

1. 三种身份继续分离：
   - Music Value：`ObjectId`，同参同定义同上下文的函数结果共享；
   - Function Invocation：调用点 + 实例序号 + 深度 + Trace 记录；
   - Performance Instance：`InstanceId`，每个 Clip 独立的表情与自动化。
2. 缓存键加入度量上下文：
   ```text
   fn:<name>:v<version>:k<key>:mc<mapId>@<anchor>|args...
   ```
   - `mapId` 为 `base` 或 `section:<index>`；
   - `anchor` 为解析 bars/beats 的局部位置。
3. 函数定义版本仍为结构哈希；定义变化改变版本，从而改变缓存键与生成结果。
4. 调用链：被调用函数的版本变化会改变其节点；调用者在本编译内重新求值，因此结果随调用链变化。
5. 无关上下文变化：
   - 不同 Section 的 MeterMap 使用不同 mapId，不会错误共享；
   - 同一 Section 的无关编辑不改变其 mapId/anchor；
   - Key/Meter 的上下文失效仍由 Music IR 的 `ContextAspect` 管理。
6. Trace 的 `functionCalls` 增加 `metricContext`（mapId + anchor + 生效拍号）。

## 理由

- 保持现有纯函数与 Music IR 依赖模型；
- 不把 Section 位置塞进 ObjectId 或 Clip InstanceId；
- 缓存键完整描述求值所依赖的度量输入。

## 后果

- 同一函数在 7/8 与 5/8 Section 产生不同对象，可能减少跨 Section 缓存命中；这是正确性优先。
- 跨编译持久缓存仍未实现；本 ADR 只定义键的语义。
- 随机仍为显式 seed/index，不进入缓存键之外的状态。
