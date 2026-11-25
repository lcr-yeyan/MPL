# Frame Contract Specification（V0）

状态：V0.4.0-beta

代码：`include/mpl/lab/Task.hpp`、`src/lab/Task.cpp`

## 1. 四类契约

| 契约 | 问题 | 失败结果 |
| --- | --- | --- |
| Target | 目标观察是否满足？ | fail / unverifiable |
| Preserve | 保持观察是否仍等于 baseline？ | fail / unverifiable |
| Frame | 改动是否在授权作用域内？ | fail / unverifiable |
| Validity | baseline/candidate 是否成功编译/执行？ | error |

验证结果分别记录：

```json
{
  "status": "pass|fail|unverifiable|error|not-applicable",
  "validityStatus": "ok|failed",
  "targetStatus": "pass|fail|unverifiable",
  "preserveStatus": "pass|fail|unverifiable",
  "frameStatus": "pass|failed|restricted|unrestricted|not-applicable"
}
```

## 2. 任务字段

```json
"frame": {
  "allow": [
    {"object": "clip", "key": "Keys/Keys#0", "property": "symbolicEvents"},
    {"object": "production", "property": "automation", "cause": "dependency"}
  ],
  "deny": [
    {"object": "production", "key": "*", "property": "tempo"}
  ]
}
```

- `object`：`track` / `clip` / `section` / `harmony` / `production`；`*` 通配。
- `key`：精确 key 或 `*`。
- `property`：精确属性或 `*`。
- `cause`：`*` / `direct` / `dependency` / `unknown`。
- `deny` 优先于 `allow`。
- 若 `frame` 存在但没有 `allow`，默认不允许任何变化。
- 若 `frame` 不存在：
  - `allowedScope=program`：不限制；
  - `allowedScope=none`：要求源完全相同。

## 2.1 覆盖范围

`frameCoverage()` 定义可授权的属性：

- track: instrument / role / assetBindings；
- clip: start / duration / symbolicEvents / instanceId / sourceObject；
- section: start / duration；
- harmony: timeline；
- production: tempo / meter / logicalExtent / mix / automation / temporalCurves。

规则引用未覆盖对象/属性时，`frameStatus=unverifiable`，不得 pass。`objectIds` 不在
value 语义内，不作为内容变化。

## 3. 执行

1. 编译 baseline/candidate；失败 → validity failed。
2. 计算 `SemanticDelta`。
3. 对每个 change 匹配 `deny`，再匹配 `allow`。
4. 未授权 → 生成 `frame:<object>.<property>` 比较项，包含：
   - 变更种类；
   - 对象 key；
   - 属性；
   - before / after；
   - source；
   - cause（direct / dependency / unknown）。
5. 全部授权 → `frameStatus=pass`。
6. 目标/保持比较独立进行；任一 fail 则总 fail，任一 unverifiable 则总 unverifiable。

## 4. 依赖传播

共享依赖修改会在 Semantic Delta 中产生多个对象变化，全部按 `dependency` 分类。Frame
逐对象检查；不允许“只检查第一处”而放过传播影响。

## 5. Unverifiable

- Flat Event 候选无法计算生产级 Semantic Delta：若任务声明 `frame`，结果为
  `not-applicable`，不是 pass。
- Semantic Delta 的 `unobserved` 属性不由 Frame 声称未变；V0 已把 objectIds 之外的主要
  生产属性纳入比较。
- 音频/渲染作用域由 Trusted Render Manifest 单独验证。

## 6. 负例

`tests/test_lab.cpp`：

- `FrameContractRejectsUnauthorizedClipEdit`：只允许 track.instrument，clip 改动失败；
- `FrameContractAllowsDeclaredClipEdit`：允许 clip.symbolicEvents 时通过；
- `TaskVerifierRejectsScopeNoneChange`：`allowedScope=none` 修改失败；
- `TaskVerifierRejectsPreserveViolation`：保持契约失败；
- `TaskVerifierRejectsUnchangedCandidateForEditTask`：目标未完成失败。

## 7. 限制

- V0 的 key 是语义路径，不是稳定对象 ID；
- 不支持声明级/区域级 selector 语言；
- 不支持对未观察属性做“证明不变”；
- Frame 不替代 target/preserve；三者独立。
