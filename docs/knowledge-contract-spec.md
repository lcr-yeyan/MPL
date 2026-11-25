# Knowledge Contract Specification（V0.5.0-alpha）

## 1. 目标

把知识单元的声明连接到现有 Observation Registry、Compiler、Task Verifier、
Semantic Delta 与 Frame Contract。验证是**契约检查**，不是通用数学证明。

## 2. 入口

```cpp
KnowledgeVerification verifyKnowledge(const KnowledgeUnit&, const KnowledgeRegistry&);
```

## 3. 检查类型

每个 `KnowledgeCheck`：

- `phase`：precondition / postcondition / invariant / effect；
- `program`：完整 MPL 程序；
- `observation`：Observation Registry ID；
- `comparator`：equals / gte / lte / tolerance；
- `expected`：期望 JSON；
- `tolerance`：数值容差。

Verifier 编译程序、计算观察、比较期望。

## 4. 状态

| 状态 | 含义 |
| --- | --- |
| Pass | 所有检查通过且知识声明 verified |
| Fail | 任一检查不通过，或示例不编译 |
| Unverified | 没有可执行检查，或检查不可验证，或声明本身 unverified |
| Unsupported | 观察/知识能力不支持，或知识声明 unsupported/deferred |
| Error | 编译/依赖/基础设施错误 |

Unknown/Unverified 永不算 Pass。

## 5. 覆盖

- 33/33 示例编译；
- 31/33 单元契约 pass；
- 36 个观察检查（postcondition/invariant）；
- 前置条件目前为声明式字符串，未全部机器执行；因此报告不宣称“所有前置条件已验证”。

## 6. 故障注入

测试：

- 把 `event_count` 期望改为 99 → Fail；
- 未知 observation → Unsupported；
- practice 单元 → Unverified；
- deferred 单元 → Unsupported。

这证明验证器能发现至少一种 postcondition 违反，而不是只接受实现自报。

## 7. 允许影响

`allowedEffects` 是声明式对象/属性列表，用于 Knowledge Resolver 的兼容过滤；
它不等同于 Frame Contract。真正的任务作用域仍由 Frame Contract 验证。

## 8. 限制

- 不支持任意谓词/约束语言；
- 不验证音乐审美；
- 不证明实现对所有输入正确；
- 前置条件未全部执行；
- 检查的程序是示例，不是形式化定理。
