# Knowledge Resolution Specification（V0.5.0-alpha）

## 1. 输入

`TaskRequirementSet`（见 `task-requirement-spec.md`）与 `KnowledgeRegistry`。

## 2. 匹配

对每个知识单元计算确定性分数：

- target observation 重叠：+4/项；
- preserve observation 重叠：+2/项；
- 输入/输出类型重叠：+1/项；
- category 与 domain 相同：+3；
- allowed effect 重叠：+2/项。

`maxMatches` 截断，截断项进入 `unresolved`。

## 3. 状态

| 状态 | 条件 |
| --- | --- |
| Applicable | 有目标/保持/类型重叠，依赖存在，知识 verified |
| ConditionallyApplicable | 有重叠但知识 unverified，或 practice 建议 |
| Inapplicable | 无重叠、无 allowed-effect 重叠、unsupported/deferred |
| Unknown | 依赖缺失或需求未知 |

`practice` 知识永远是 Conditional，不得变成硬约束。

## 4. 原因

每个 match 带 `reasons`：target/preserve/type/domain/effect 匹配或缺失依赖。
排序：分数降序，再 `knowledgeId` 升序。

## 5. 重要限制

- 检索结果不等于任务满足；
- 分数不是成功概率；
- 所有候选仍必须编译并通过 target/preserve/frame/validity；
- 规则评分版本 `1`，变更需新版本与测试；
- 不做自然语言检索或 embedding。

## 6. API

`KnowledgeResolution::toJson()` 输出 `mpl.knowledge-resolution` v1。

## 7. 测试

- 确定性（同输入同 JSON）；
- effect 不兼容过滤；
- 缺失依赖 → Unknown；
- domain/category 匹配；
- 多知识域依赖闭包（context 构建）。
