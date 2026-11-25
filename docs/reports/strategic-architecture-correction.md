# Strategic Architecture Correction（V0.5.0-alpha）

日期：2026-11-25

## 1. 暂停的模型专用方向

以下工作**暂停**，不进入 V0.5.0-beta 核心路线：

- 模型专用任务分解器；
- 模型专用程序草图合成器；
- 自主合成规划器；
- 模型专用知识引导修复；
- 以 Prompt 优化为核心的 Adaptive Context 扩展；
- 多阶段 LLM 合成工作流。

仓库中已存在的相关代码/实验**未删除**：

- `src/lab/ModelAdapters.cpp`、`Harness`、`Repair`、`Statistics`；
- `KnowledgeResolver`、`AdaptiveContext`；
- `lcp/v1`、`lcp/v1.1`；
- V0.4/V0.5 的原始实验记录；
- 模型调用原始输出。

它们保存在可选 `mpl_experiment` 库与实验记录中，不影响 core。

## 2. 项目定位

MPL 是独立的音乐生产编程语言、编译器、运行时和开发工具链，不是音乐 Agent、
Prompt 框架或特定模型代码生成系统。

模型与人类开发者、传统软件一样，是 MPL 的使用者；MPL 核心语义、Stdlib、运行时
不依赖任何模型。

## 3. 模块重定位

| 模块 | 新定位 | 构建层 |
| --- | --- | --- |
| Music Knowledge Registry | 音乐计算知识/Stdlib 语义索引/语言反射 | `mpl_knowledge` |
| Knowledge Contracts | 通用函数契约与音乐验证 | `mpl_knowledge` |
| Observation Registry | 核心语义观察 | `mpl_core` |
| Semantic Delta | 语义差分 | `mpl_core` |
| Frame Contract | 作用域/保持契约 | `mpl_core` |
| Trusted Execution | 受控渲染与来源证据 | `mpl_core` |
| Model Adapter / LCP / Adaptive Context / Resolver | 独立实验层 | `mpl_experiment` |

## 4. 已执行的架构修正

- `mpl_core` 删除 model adapter、harness、repair、statistics、resolver、adaptive context；
- 新增 `mpl_knowledge`（Registry、TaskRequirements、Verifier），不依赖模型；
- 新增 `mpl_experiment`（ModelAdapter、Harness、Repair、Statistics、Resolver、AdaptiveContext），
  唯一链接 `winhttp`；
- `mplc` 只链接 core/demo；
- `mpl_lab` 是开发/实验聚合工具，不属于 compiler/runtime；
- 331/331 测试保持通过。

详见 `docs/reports/architecture-independence-audit.md`。

## 5. 独立于模型的价值评估

| 组件 | 独立价值 | 决策 |
| --- | --- | --- |
| Knowledge Registry | 高：语义索引/反射/版本 | 保留并核心化（独立库） |
| Knowledge Contracts | 高：通用函数/音乐验证 | 保留并优先发展 |
| Observation/Delta/Frame/Trusted | 高：通用语义基础设施 | 保留在 core |
| LCP | 中：实验上下文材料 | 实验层，不进入编译器 |
| Adaptive Context/Resolver | 中/低：依赖模型行为 | 暂停扩展，保留记录 |
| Model Adapter/Harness/Repair | 低：实验工具 | 实验层，不进入 core |
| Statistics | 中：通用统计方法 | 实验层，可复用 |

## 6. 不做什么

- 不因模型偏好增加语法；
- 不因模型难用删除合理音乐语义；
- 不把 Prompt/知识建议作为语言合法性；
- 不删除历史失败数据；
- 不把实验层反向链接进 core。

## 7. 下一步

1. 以 `language-expressiveness-audit.md` 的 P0/P1 为 V0.6 语言候选；
2. 将 Knowledge Contract 发展为通用函数契约（符号解析、pre/post、负例）；
3. 完成 Stdlib 缺口审计与通用库补充；
4. 保持 Adaptive Context 仅作实验，不进入核心验收。
