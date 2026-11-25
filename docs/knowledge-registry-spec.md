# Knowledge Registry Specification（V0.5.0-alpha）

## 1. 实现

`include/mpl/knowledge/Knowledge.hpp`、`src/knowledge/Knowledge.cpp`。

注册表是内存索引 + 本地 JSON 目录，不依赖向量数据库、远程服务或 LLM 检索。

## 2. 加载

```text
knowledge/v1/catalog.json
```

格式：`mpl.knowledge-catalog`，`units` 数组。加载时：

1. 解析字段并校验必填项；
2. 拒绝重复 `(knowledgeId, version)`；
3. 建立 category / kind / input-output type / observation / tag 索引；
4. 计算 `contentHash`；
5. 递归计算依赖 `resolvedHash`。

任一 missing/cycle 使加载失败，并记录 `knowledge-missing-dependency` /
`knowledge-cycle` / `knowledge-duplicate`。

## 3. 解析

- `resolve(id, version="")`：精确版本或第一个匹配；
- `byCategory` / `byType` / `byObservation` / `byTag`：稳定排序；
- `dependencyClosure(id)`：依赖优先的拓扑序；
- `missingDependencies(id)`：直接和传递缺失；
- `toJson()`：按 `id@version` 稳定排序。

## 4. 哈希

- `contentHash = SHA-256(canonical unit JSON without hashes)`；
- `resolvedHash = SHA-256(contentHash + dependency resolvedHash...)`。

依赖内容变化会改变父单元 `resolvedHash`，即使父单元自身内容不变。

## 5. 验证状态

`verified` / `unverified` / `unsupported` / `deferred` / `failed`。
状态是知识声明的一部分，契约验证另见 `knowledge-contract-spec.md`。

## 6. 确定性

相同 catalog、相同版本与配置必须产生相同索引顺序、闭包和序列化。测试覆盖：

- 重复 ID；
- 缺失依赖；
- 循环依赖；
- 传递依赖哈希变化；
- 稳定 `toJson()`。

## 7. 限制

- 初期只支持单一 catalog；
- 不做跨 catalog 合并；
- 不提供远程检索/向量索引；
- `resolvedHash` 不是签名。
