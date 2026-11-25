# Executable Contracts（V0.5.0-beta）

## 1. 已有基础

- Knowledge Contract Verifier：`verifyKnowledge`，编译程序、计算 Observation、比较 expected；
- Task Verifier：target/preserve/frame/validity；
- Frame Contract；
- Stdlib 函数契约：`stdlib/contracts.json`。

## 2. 状态

| 状态 | 含义 |
| --- | --- |
| Pass | 检查通过 |
| Fail | 检查不通过 |
| Unverified | 没有机器检查或声明未验证 |
| Unsupported | 观察/能力不支持 |
| Error | 编译/运行/依赖错误 |

Unknown/Unverified 不自动成为 Pass。

## 3. 新 Stdlib 契约

`StdAnalysis.pitchClasses`、`intervalsOf`、`spanOf`；
`StdVoicing.topVoice`、`bottomVoice`、`innerVoices`。

每个记录：

- type signature；
- preconditions；
- postconditions；
- invariants；
- complexity；
- negative case；
- 测试证据。

## 4. 失败案例

- `topVoice` 空列表：实际触发 `index-negative`/`index-out-of-range`，测试接受二者之一；
- `intervalsOf` 单音：返回空列表；
- `innerVoices` 双声部：返回空列表；
- 知识契约仍覆盖 postcondition/invariant；前置条件多数是声明式。

## 5. 限制

- 不是通用定理证明；
- 没有语言级 `requires`/`ensures` 语法；
- 前置条件未全部自动执行；
- 契约检查是示例/程序级 Observation，不是符号执行；
- 复杂算法的完备正确性不在本阶段。
