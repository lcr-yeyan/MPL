# MPL Language Context Package（MPL-LCP）Specification

版本：1.1  
语言：0.3.0  
Stdlib：0.3.0  
编译器：0.4.0

## 1. 目标

MPL-LCP 是给外部模型的语言上下文包，不是语言实现的一部分。它必须：

- 从冻结规范生成，不引入未实现语法；
- 所有示例由冻结编译器验证；
- Core 与 Stdlib 独立构建；
- 每次修改产生新版本、SHA-256 与 Token 估计；
- 不在 held-out Prompt 中包含参考解。

## 2. 组成

| 组件 | 文件 |
| --- | --- |
| Grammar Reference | `lcp/v1.1/core/10-grammar.md` |
| Type Reference | `lcp/v1.1/core/20-types.md` |
| Builtin Signatures | `lcp/v1.1/core/30-builtins.md` |
| Verified Canonical Examples | `lcp/v1.1/core/60-examples.md` + `examples/*.mpl` |
| Semantic Contract | `lcp/v1.1/core/40-semantics.md` |
| Diagnostic Guide | `lcp/v1.1/core/50-diagnostics.md` |
| Output Protocol | `lcp/v1.1/core/00-output.md` |
| Stdlib (only C arms) | `lcp/v1.1/stdlib/10-stdlib.md` |

Core arms **不得**接收 `stdlib/10-stdlib.md`。Stdlib 是实验因素，不是通用知识。

## 3. Context Variants

| Variant | 内容 | 用途 |
| --- | --- | --- |
| L0 | Output protocol | 最小上下文 |
| L1 | L0 + grammar + types | 语法/类型诊断 |
| L2 | L1 + verified examples | 示例效应 |
| L3 | L2 + builtins + semantics + diagnostics | 机器可读契约 |
| L4 | L3（运行时追加结构化反馈） | 反馈条件 |
| L3-control | L3 + 无诊断的重试 | 匹配重试控制 |
| Core | L3 | B0/B1 |
| C | Core + stdlib | C0/C1 |
| A | Flat Event schema | A0/A1 |

## 4. 版本与身份

`lcp/v1.1/contexts/manifest.json` 记录：

- 每个组件的 SHA-256；
- 每个变体的 SHA-256、字节数、估算 Token；
- 每个示例的 SHA-256 与 `verified=true`；
- 生成方式、语言/Stdlib/编译器版本。

Token 估计为 `ceil(UTF-8 bytes / 4)`，**不是**模型 tokenizer 精确值；实际调用 Token 由
API usage 记录。

## 5. 示例验证

所有 `examples/*.mpl` 必须通过：

```text
mplc check lcp/v1.1/examples/<name>.mpl
```

示例不得与开发任务答案完全相同；v1.1 已替换 basic/meter/function/voice 示例的音高，
避免 L2 直接复制答案。

## 6. v1.1 修正

v1 的 Learnability 暴露：

- 模型使用 `pitches[0]`，但 MPL 不支持列表索引；
- 示例中的 `track Keys` 被模型复制到 local-edit，导致 frame 越权；
- L2 示例缺失合法的 voicing+rhythm 组合。

v1.1 增加：

- 明确“禁止列表索引”；
- 明确“编辑任务保留所有标识符，示例名称不可复制”；
- 一个使用 `cycle(R, voicingNotes(V), velocity)` 的已验证示例。

v1.1 结果：L2 3/6 → 5/6，L3 4/6 → 5/6；local-edit frame 仍需 prompt 明确写出
track/clip 名后才能通过。

## 7. 限制

- 不是自动生成器；修改需人工审查并更新 manifest；
- Token 估计不是精确 tokenizer；
- LCP 不含 held-out 参考解、任务 target 值或音频；
- 语言语义仍以编译器为准，示例不构成新语法。
