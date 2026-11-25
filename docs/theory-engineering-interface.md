# Theory–Engineering Interface

版本：1  
文件：`theory/v1/interface.json`

## 1. 目的

显式记录 UTPMP 理论主张与工程实现的对应关系，避免把理论草案自动当作已证明结论，
也避免要求理论研究必须服从当前实现。

## 2. 字段

- `theoryClaimId`
- `theoryVersion`
- `implementationRef`
- `assumedSemantics`
- `conformanceStatus`
- `verificationEvidence`
- `knownCounterexamples`
- `limitations`

## 3. 分类

| 状态 | 含义 |
| --- | --- |
| `conforms-by-design` | 工程语义按该假设实现，有测试证据 |
| `implementation-approximation` | 工程是受限近似，例如有界声部搜索 |
| `candidate` / `unverified` | 理论候选或尚未验证 |
| `conflict` | 已知冲突，需迁移评估 |

## 4. 当前条目

- Meter bars 锚定；
- Rhythm common period；
- voice-leading 成本与有界搜索（明确 budget exhaustion ≠ unsat）；
- harmony 右连续/重叠；
- adaptive context 的成本假设（未验证）。

## 5. 反例与迁移

如果 Theory Validation 发现反例：

1. 更新 `knownCounterexamples`；
2. 标记受影响 `implementationRef`；
3. 新增 ADR/回归测试；
4. 不静默改写冻结实验基线；
5. 评估是否需要新语言版本。

## 6. 限制

- 接口是记录，不是自动定理证明；
- 不要求理论草案与工程完全一致；
- 不把 `conforms-by-design` 解释为数学证明。
