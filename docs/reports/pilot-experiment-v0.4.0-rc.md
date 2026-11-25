# Pilot / Learnability Experiment Report（V0.4.0-rc）

日期：2026-11-25

## 1. 执行状态

- 真实模型：DeepSeek OpenAI-compatible；请求 `deepseek-chat`，供应商返回 `deepseek-flash`。
- 已执行：8 次 smoke、2 轮 Learnability（L0–L4 + 控制）、1 次 prompt 修正测试、
  30 次六组 Pilot。
- 总计：**117 次真实模型调用**（见汇总）。
- 所有结果由独立验证器计算；无手工改判。

## 2. Learnability

任务（6）：basic、meter、theme、function、voice-leading、local-edit。
每条件 n=6；L4 与控制组最多 2 次尝试。

### LCP v1

| 条件 | 首次通过 | 最终通过 | 语法有效 | 类型有效 | 失败分类 |
| --- | --- | --- | --- | --- | --- |
| L0 | 0/6 | 0/6 | 1/6 | 1/6 | 5 syntax，1 frame |
| L1 | 2/6 | 2/6 | 4/6 | 3/6 | 2 syntax，1 type，1 frame |
| L2 | 3/6 | 3/6 | 6/6 | 5/6 | 1 type，1 preserve，1 frame |
| L3 | 4/6 | 4/6 | 5/6 | 5/6 | 1 syntax，1 frame |
| L4 | 4/6 | 4/6 | 5/6 | 5/6 | 1 syntax，1 frame |
| L3-control | 4/6 | 4/6 | 5/6 | 5/6 | 1 syntax，1 frame |

### LCP v1.1

v1.1 增加了“列表不可索引”与“编辑任务保留标识符”，并替换了会泄露答案的示例。

| 条件 | 首次通过 | 最终通过 | 语法有效 | 类型有效 | 失败分类 |
| --- | --- | --- | --- | --- | --- |
| L0 | 0/6 | 0/6 | 1/6 | 1/6 | 5 syntax，1 frame |
| L1 | 2/6 | 2/6 | 4/6 | 3/6 | 2 syntax，1 type，1 frame |
| L2 | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |
| L3 | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |
| L4 | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |
| L3-control | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |

L4 与控制组相同，因为唯一失败（local-edit）的 frame 违反在二次反馈后仍未修复；
模型重复把 `track Piano` 改为 `track Keys`。

### Prompt 修正

在 local-edit 提示中明确写出“track Piano、clip ... 不变，不要复制示例名称”后，
单次 B0 调用 **pass**。说明该失败是上下文/示例名称诱导，而非语言能力缺陷。

## 3. 六组 Pilot

5 个任务（basic、meter、multi-period、theme、voice-leading）× 6 arms = 30 次调用。

| Arm | n | pass | fail | 输入 tokens | 输出 tokens | 平均延迟 |
| --- | --- | --- | --- | --- | --- | --- |
| A0 | 5 | 5 | 0 | 1,972 | 1,104 | 1,182 ms |
| A1 | 5 | 5 | 0 | 1,972 | 1,104 | 1,379 ms |
| B0 | 5 | 5 | 0 | 15,845 | 914 | 1,560 ms |
| B1 | 5 | 5 | 0 | 15,845 | 914 | 1,357 ms |
| C0 | 5 | 5 | 0 | 17,960 | 914 | 1,333 ms |
| C1 | 5 | 5 | 0 | 17,960 | 914 | 1,422 ms |

合计：30/30 pass、77,418 tokens、41,171 ms。

**这是 ceiling effect：** 每个任务 prompt 都明确给出了目标音高/起音/时值，模型基本是
转录而非泛化合成。因此不能据此声称 MPL 或 Flat 表示更优，也不能作为确认性结论。
任务聚类的配对差异全为 0，95% bootstrap CI 为 [0,0]。

## 4. 失败审计

### 4.1 Voice-leading 列表索引

v1 中多个候选使用 `pitches[0]`；MPL 不支持列表索引，报
`parse-expected-rparen`。LCP v1 未明确禁止；v1.1 增加禁止说明和一个
`cycle(R, voicingNotes(V), velocity)` 合法示例后，L2+ 通过。

### 4.2 Local-edit frame 违反

模型输出目标音高正确、事件/起音/长度保持正确，但把 `track Piano` 改为
`track Keys`（受示例名称诱导）。验证器报告：

```text
frame:track.track fail — unauthorized removed 'Piano'
frame:track.track fail — unauthorized added 'Keys'
frame:production.mix fail — mix track map changed
```

targetStatus=pass、preserveStatus=pass、frameStatus=failed。说明 frame contract
能捕获“目标对但作用域越权”的候选。

### 4.3 L0 语法失败

L0 仅输出协议，5/6 出现 `parse-*`；级联诊断多由缺少 `=`、`;`、`{}` 或错误 clip 语法引起。
这些候选从未被计为成功。

## 5. 总调用与 Token

| 实验 | Calls | Tokens |
| --- | --- | --- |
| Smoke（8） | 8 | 3,031 |
| Learnability v1 | 40 | 89,243 |
| Learnability v1.1 | 38 | 91,903 |
| Prompt-v2 local-edit | 1 | 3,404 |
| Pilot 六组 | 30 | 77,418 |
| **合计** | **117** | **264,999** |

精确 Token 以 `bench/results/**/result.json` 与 summary 为准；总计延迟 169,828 ms。

## 6. 结论

- 真实模型、adapter、LCP、独立验证器、frame、repair 路径均已跑通；
- LCP 示例/禁止项对可学习性有可观察影响（L2 3/6 → 5/6）；
- frame 能识别“目标满足但越权”；
- Pilot 因提示词泄露目标而 ceiling，不能用于表示机制比较；
- 正式 held-out 实验尚未执行。

## 7. 安全

API key 曾在对话中暴露；虽然用户表示不担心余额，仍建议轮换。产物未写入 key。
