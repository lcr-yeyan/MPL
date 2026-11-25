# ADR-0028：有界声部连接搜索与状态分类

状态：已采纳（V0.3.0-rc）

## 背景

声部连接需要明确硬约束、优化目标、搜索预算与失败语义。把“预算耗尽”当成“无解”会
误导用户，也违反可复现要求。

## 决策

1. 硬约束：声部数、音域、和弦音、顺序/相邻间距（可选）、每声部最大移动。
2. 优化目标：总半音移动距离最小。
3. 平局：低到高 pitch 列表字典序更小者优先。
4. 搜索：DFS，候选按移动距离/MIDI 排序；预算默认 20000 节点；确定性。
5. 状态：
   - `strictly-satisfied`
   - `satisfied-within-tolerance`（+1 半音）
   - `unsatisfied`
   - `backend-capability`
   - `budget-exhausted`
   - `provably-unsatisfiable`
6. `budget-exhausted` 与 `provably-unsatisfiable` 必须分开；前者不声称无解。
7. `allowCrossing` 控制是否允许交叉；交叉本身不是错误。
8. 重复音/省略音由 voicing 规则产生；`complete` 是信息，不是默认错误。

## 后果

- 搜索失败可解释、可复现。
- 预算可调，但状态语义不随预算改变。
- 不引入通用约束求解器或全局优化框架。
