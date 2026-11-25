# MPL-Learnability-001 Report

日期：2026-11-25

## 1. 设计

- 模型：`deepseek-flash`（请求 `deepseek-chat`）；
- 开发集 6 任务：basic、meter、theme、function、voice-leading、local-edit；
- 每条件 n=6；
- L0–L3 一次尝试；L4 与控制组最多 2 次；
- 控制组：L3 上下文 + 第二次重试，但不提供结构化诊断；
- 任务/prompt/context 均冻结并有 SHA-256。

## 2. LCP v1

| 条件 | 首次通过 | 最终通过 | 语法有效 | 类型有效 | 失败分类 |
| --- | --- | --- | --- | --- | --- |
| L0 | 0/6 | 0/6 | 1/6 | 1/6 | 5 syntax，1 frame |
| L1 | 2/6 | 2/6 | 4/6 | 3/6 | 2 syntax，1 type，1 frame |
| L2 | 3/6 | 3/6 | 6/6 | 5/6 | 1 type，1 preserve，1 frame |
| L3 | 4/6 | 4/6 | 5/6 | 5/6 | 1 syntax，1 frame |
| L4 | 4/6 | 4/6 | 5/6 | 5/6 | 1 syntax，1 frame |
| L3-control | 4/6 | 4/6 | 5/6 | 5/6 | 1 syntax，1 frame |

Token：89,243；延迟：58,656 ms；calls 40。

## 3. LCP v1.1

v1.1 修正：

- 明确禁止列表索引；
- 明确编辑任务保留标识符；
- 增加合法 voicing+rhythm 示例（非任务答案）。

| 条件 | 首次通过 | 最终通过 | 语法有效 | 类型有效 | 失败分类 |
| --- | --- | --- | --- | --- | --- |
| L0 | 0/6 | 0/6 | 1/6 | 1/6 | 5 syntax，1 frame |
| L1 | 2/6 | 2/6 | 4/6 | 3/6 | 2 syntax，1 type，1 frame |
| L2 | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |
| L3 | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |
| L4 | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |
| L3-control | 5/6 | 5/6 | 6/6 | 6/6 | 1 frame |

Token：91,903；延迟：57,361 ms；calls 38。

## 4. 失败归因

- L0：语法不足；5/6 `parse-*`；
- L1：theme/function/voice-leading 仍语法错误；local-edit frame 失败；
- L2 v1：voice-leading 列表索引幻觉；local-edit frame；theme preserve；
- v1.1：voice-leading 修复；local-edit 仍把 `track Piano` 改名 `Keys`；
- L4 与控制组相同：唯一失败是 frame 越权，诊断反馈未能修正名称遵循。

### Prompt-v2

在 local-edit 明确写出 `track Piano`、`clip ... "Basic"` 不变并禁止复制示例名称后，
B0 单次调用 pass（3,246 input + 158 output，1,398 ms）。

## 5. 解释

- L2 的 3/6→5/6 改善与示例/禁止项修正同时发生，不能单独归因；
- L4 没有超过 L3/control，说明反馈对“名称遵循”失败无效；
- 任务数仅 6，属于开发集诊断，不是确认性统计；
- LCP 优化使用了开发集，符合“开发集用于上下文优化”。

## 6. 产物

- `bench/results/learnability-001/`
- `bench/results/learnability-002/`
- `lcp/v1/`、`lcp/v1.1/` 与各自 manifest
- `docs/mpl-lcp-spec.md`
