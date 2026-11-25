# MPL 随机流与算法版本契约（V0.3.0-beta）

状态：契约已定义；最小原语已实现，局部流派生 API 未实现。

## 1. 目的

让“可复现的生成”不依赖编译顺序、线程调度、系统时间或全局可变状态，并为未来跨编辑
稳定随机流与算法升级保留明确版本边界。

## 2. 已实现原语

```text
random(seed: Int, index: Int) -> Number
```

实现：

```text
state = uint64(seed) XOR (uint64(index) * 0x9e3779b97f4a7c15)
state += 0x9e3779b97f4a7c15
state = (state XOR (state >> 30)) * 0xbf58476d1ce4e5b9
state = (state XOR (state >> 27)) * 0x94d049bb133111eb
state = state XOR (state >> 31)
value = (state >> 11) / 2^53        // [0,1)
```

- 纯函数；相同 `seed/index` 跨平台、跨编译顺序一致；
- 无全局随机状态；
- 每次调用携带显式 seed/index；
- 结果参与函数缓存键（通过实参）与 Trace（通过调用记录）。

## 3. 算法版本

- 当前算法标识：`splitmix64`，版本 `1`；
- Trace 的 `random` 字段记录 `algorithm`、`version` 与 `primitive`；
- 任何改变输出映射的修改都必须提升版本号；
- 缓存/持久化结果必须同时以算法版本为键；版本不同不得复用；
- 版本升级属于语义变更，不是格式变更。

## 4. 局部可派生随机流（契约，未实现）

未来 API：

```text
randomStream(seed: Int, streamId: String, algorithmVersion: Int) -> Stream
stream.next(index: Int) -> Number
```

派生公式（建议）：

```text
streamSeed = mix64(seed XOR hash(streamId) XOR (algorithmVersion * golden))
```

规则：

- `streamId` 必须由显式语义标识组成，例如 `"section:B/voice:lead/v1"`；
- 不允许依赖编译顺序、内存地址、时间或线程；
- 同一 `seed + streamId + algorithmVersion + index` 必须产生相同结果；
- 不同声部/段落使用不同 streamId，避免共享 seed 时的隐式相关；
- Trace 记录 streamId 与算法版本；
- 未实现的 API 不在 V0.3.0-beta 暴露，避免“看起来支持但语义未定义”。

## 5. 与函数身份的关系

- 随机原语是纯函数；函数调用身份仍由调用点 + 实例 + 度量上下文记录；
- 同一函数同参调用共享 Music Value，不改变随机结果；
- 独立 Clip 的 PerformanceExpression 与自动化仍独立；
- 函数定义版本变化会改变缓存键；随机算法版本是独立的全局契约版本。

## 6. 边界

- 不做概率节奏、风格律动或 humanize 模型；
- 不以随机数代替明确音乐决策；
- 不允许无种子随机或隐式全局随机源；
- 不在本阶段实现跨编辑稳定随机流的完整 API。
