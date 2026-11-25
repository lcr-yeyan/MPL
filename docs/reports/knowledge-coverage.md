# Knowledge Coverage Report（V0.5.0-alpha）

## 1. 单元数量

`knowledge/v1/catalog.json`：**33** 个知识单元。

| 类别 | 数量 |
| --- | --- |
| Rhythm | 6 |
| Melody | 5 |
| Harmony | 5 |
| Voicing | 6 |
| Arrangement | 6 |
| Production | 5 |

## 2. 类型

| kind | 数量 |
| --- | --- |
| executable | 15 |
| construction | 11 |
| relation | 3 |
| verification | 3 |
| practice | 1 |

`practice.voice-leading-preference` 是 advisory，不自动成为硬约束。

## 3. 契约与验证

| 项 | 值 |
| --- | --- |
| 单元 | 33 |
| 有可执行观察检查 | 31 |
| 自动验证 pass | 31 |
| unverified | 1（practice） |
| deferred / unsupported | 1（motivic development） |
| 编译通过的示例 | 33/33 |
| 观察检查总数 | 36 |
| 依赖边 | 34 |
| verified 覆盖率 | 31/33 = 93.9% |

## 4. 未覆盖/未验证

- `arrangement.motivic-development`：deferred；V0.4 没有通用动机发展算法。
- `practice.voice-leading-preference`：unverified；advisory，不是硬约束。
- 前置条件目前主要是声明式，未全部机器执行；检查主要覆盖 postcondition/invariant。
- 36 个检查多为示例级观察断言，不是形式化定理证明。
- 没有为每个知识单元提供独立负例程序；负例主要通过故障注入测试验证。

### 4.1 实现引用审计

| 引用 | 数量 |
| --- | --- |
| `core:` | 19 |
| `syntax:` | 7 |
| `stdlib:` | 3 |
| `observation:` | 1 |
| `task:` | 1 |
| `benchmark:` | 1 |
| `deferred:` | 1 |

19 个 `core:` 中有多个是语义别名（如 `core:voicing` 同时服务
inversion/range/spacing），不是独立符号。7 个 `syntax:` 是语言结构，不是函数。
因此 **31 verified 表示示例级契约检查通过，不等于 31 个独立可调用符号已逐一验证**。
详见 `docs/reports/standard-library-audit.md`。

## 5. 依赖

依赖闭包覆盖 Rhythm→Melody、Harmony→Voicing、Arrangement、Production。
`harmony.arp` 是多域依赖示例：
`rhythm.hit-rest`、`harmony.query`、`harmony.voicing` → `harmony.arp`。

## 6. 结论

初始知识库具备可执行引用、依赖、示例、观察契约与验证记录；但“全部前置条件已证明”不成立，
必须按上述覆盖范围引用。
