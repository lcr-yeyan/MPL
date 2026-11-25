# Model Accessibility Specification

状态：V0.4.0-rc 实证就绪

## 1. 接入方式

### 离线

`StaticModelAdapter` 或导入候选文件；无需 API。

### OpenAI-compatible HTTP

`OpenAiCompatibleAdapter` 读取环境：

- `MPL_MODEL_API_KEY`（必需，仅环境变量）
- `MPL_MODEL_BASE_URL`（默认 `https://api.openai.com/v1`）
- `MPL_MODEL_NAME`

已验证端点：

- Base URL：`https://api.deepseek.com`
- 请求 model：`deepseek-chat`
- 供应商实际返回：`deepseek-flash`
- 结果记录以返回的 `model` 为准

## 2. 安全

- key 不写入日志、实验 JSON、manifest、报告或候选文件；
- 代码不打印 Authorization header；
- 错误消息不包含 key；
- 本仓库 smoke/learnability/pilot 产物已扫描，无 key 字符串。

**注意：** key 已在对话中暴露。虽然用户表示余额可控且不担心泄露，仍建议轮换。

## 3. 执行记录

`mpl_lab model` 生成：

- `candidate-N.mpl` 或 `.json`；
- `verification-N.json`（独立验证器）；
- `result.json`（arm/task/model/tokens/latency/status）。

不记录请求文本中的 secret；prompt/context 作为实验配置由文件哈希标识。

## 4. 真实运行

| 实验 | Calls | Tokens |
| --- | --- | --- |
| Smoke | 8 | 3,031 |
| Learnability v1 | 40 | 89,243 |
| Learnability v1.1 | 38 | 91,903 |
| Prompt-v2 | 1 | 3,404 |
| Pilot | 30 | 77,418 |
| **合计** | **117** | **264,999** |

延迟合计 169,828 ms。

## 5. 限制

- 供应商返回模型与请求名不同，报告两者；
- `sampling` 以字符串记录，不假设 seed 支持；
- 未做并发/重试/超时策略的正式统计；
- API 错误、超时、预算耗尽应与任务失败分开记录。
