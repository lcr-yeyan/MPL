# Model Adapter Specification（V0.4.0-rc）

代码：`include/mpl/lab/ModelAdapter.hpp`、`include/mpl/lab/ModelAdapters.hpp`

## 1. 边界

- Adapter 不属于 MPL Core / Runtime；
- Adapter 只接收 `ModelRequest`，返回 `ModelResponse`；
- Harness 负责编译、执行、验证；
- 模型自报完成不构成通过。

## 2. 接口

```cpp
class ModelAdapter {
    virtual std::string name() const = 0;
    virtual ModelResponse generate(const ModelRequest&) = 0;
};
```

请求包含：taskId、prompt、语言/标准库上下文、上次候选、语义反馈、attempt、预算、promptVersion、sampling。

响应包含：候选源、model/modelVersion、sampling、promptVersion、输入/输出/缓存 Token、调用次数、延迟、错误。

## 3. 实现

### StaticModelAdapter

离线导入：固定候选源，用于无 API 的可复现实验。

### OpenAiCompatibleAdapter

- `MPL_MODEL_API_KEY`（仅环境变量）；
- `MPL_MODEL_BASE_URL`（默认 `https://api.openai.com/v1`）；
- `MPL_MODEL_NAME`；
- POST `/chat/completions`；
- 解析 `choices[0].message.content` 与 usage Token；
- 去除 Markdown code fence；
- 错误写入 `ModelResponse.error`，不跨边界抛出。

## 4. 密钥安全

- 密钥只从环境读取；
- 不写入日志、实验 JSON、manifest、报告或候选文件；
- 错误信息不包含 Authorization header；
- Harness 只记录 model/modelVersion/sampling/promptVersion 与 Token，不记录 key。

## 5. 采样与种子

- `sampling` 是字符串，原样记录；
- 供应商不支持随机种子时记录 `seed:unsupported`（由调用方写入）；
- 不伪造确定性。

## 6. 执行状态

V0.4.0-rc 执行了 117 次真实调用（DeepSeek OpenAI-compatible，供应商返回 deepseek-flash；264,999 tokens），包括 Learnability 与 30 次六组 Pilot。Pilot 有 ceiling effect，不作为确认性结论。
