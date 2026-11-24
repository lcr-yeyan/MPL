# MPL 算子规范（Core v0.0.3）

算子分为两类：确定性音乐变换，以及读取项目上下文（Key / Meter）的实验性算子。所有算子都是纯函数：相同输入、上下文与参数产生相同输出。

## 1. 时间与音符约定

- 所有 offset / duration / span 均为全音符有理数。
- `Fragment::fromNotes` 会按 offset 稳定排序；每个音符必须有正时值、非负偏移、力度 1..127、MIDI 0..127。
- `Fragment` 的 span 可大于内容末端，用于尾部休止与镜像语义。

## 2. 确定性算子

### transpose(fragment, interval) → Fragment

- 逐音加半音；保持偏移、时值、力度、origin、span。
- 音高越出 MIDI 0..127 时抛错。
- 拼写策略：输入含降号时优先降号拼写。

### reverse(fragment) → Fragment

- 新 offset = `span - (offset + duration)`，在 span 内镜像。
- span 不变，因此尾部休止会变成头部休止。

### stretch(fragment, factor) → Fragment

- `offset *= factor`，`duration *= factor`，`span *= factor`。
- factor 必须为正，否则抛错。

### repeat(fragment, count) → Fragment

- 第 i 份偏移 `i * span`，总 span = `count * span`。
- count < 1 抛错。

### sequence(parts...) → Fragment

- 依次拼接，后续片段整体后移前面 span 之和。
- 至少一个输入。

### parallel(parts...) → Fragment

- 所有片段从 0 开始叠加，结果 span 取最大值。
- 至少一个输入。

## 3. 上下文相关算子

### diatonicTranspose(fragment, scaleSteps) → Fragment

上下文：`key`。

语义：

1. 依据音高拼写得到相对主音的音级位置（字母级数，而非半音）。
2. 保留输入音级相对调内音的半音变化量。
3. 移动 `scaleSteps` 个音级，并重算拼写与八度。
4. 越出 MIDI 范围抛错。

示例（F minor）：

```text
F4  +2 → Ab4
Ab4 +2 → C5
C5  +2 → Eb5
Eb5 +2 → G5
B3  +1 → C#4   （B 是升高的四级，保留升高）
F4  -2 → Db4
```

### harmonize(fragment, degreeOffsets, velocity=80) → Fragment

上下文：`key`。

- 对每个输入音，按 `degreeOffsets` 生成若干音级偏移的和弦音，偏移 0 不合法。
- 生成音与原音同起点、同时值，力度由参数决定（默认 80）。
- 原音 origin 不变；生成音 origin = harmonize 节点 id。
- 至少一个偏移，力度必须在 1..127。
- 不检查和声功能、不避免声部交叉、不做声部进行优化。

示例（F minor，`{2,4}`）：

```text
F2  → F2 + Ab2 + C3
Db3 → Db3 + F3 + Ab3
Ab2 → Ab2 + C3 + Eb3
```

### syncopate(fragment, displacement) → Fragment

上下文：`meter`。

- 若音的 offset 正好落在拍点（`offset / beatLength` 为整数），则整体后移 `displacement`。
- 位移后为负则夹到 0；span 取原 span 与所有新结束位置的最大值。
- displacement = 0 时原样返回。
- 这是“拍点避让”的最小实验，不建模重音、切分张力或风格规则。

示例（4/4，位移 1/8）：

```text
0    → 1/8
1/8  → 1/8 （非拍点，不变）
1/4  → 3/8
```

## 4. 上下文与失效

| 算子 | 生成依赖 | 上下文依赖 |
| --- | --- | --- |
| literal / transpose / reverse / stretch / repeat / sequence / parallel | inputs | 无 |
| diatonic-transpose / harmonize | input | key |
| syncopate | input | meter |

修改 key 只失效 key 相关节点及其下游；修改 meter 只失效 meter 相关节点及其下游。求值顺序按对象创建 id，即天然拓扑序。

## 5. 溯源规则

- 每个音符的 `origin` 指向引入它的对象。
- 复制型算子不改 origin。
- 生成型算子（当前只有 harmonize）把新音标记为自身。
- Trace 的 clip `derivation` 给出从片段根到所有输入的链路，与 `origin` 组合即可定位生成路径。

## 6. 错误模型

| 情况 | 错误码 |
| --- | --- |
| 未知对象输入 | `transpose` / `reverse` / … |
| 非正 stretch 因子 | `stretch` |
| repeat count < 1 | `repeat` |
| 空 sequence / parallel | `sequence` / `parallel` |
| 空 harmonize 偏移 | `harmonize` |
| 音高越界 | `pitch` / `harmony` |
| 非法 key 名 | `key` |
| 非法拍号 | `meter` |
| 有理数溢出 | `rational` |

## 7. 非目标

V0.0.2 不包含：生成性算子、约束求解、风格模型、音频分析、混音/母带算子和自动配器。算子接口保持纯函数式，便于未来由 stdlib、C++ 原生算子或外部服务实现，而不把算法硬编码进编译器核心。

## 8. 高级算子的语义契约（V0.0.3）

### 8.1 DiatonicTranspose

前置条件：

- 输入 Fragment 的音高都在 MIDI 0..127。
- `scaleSteps` 为整数，可正可负。
- 上下文 Key 只能是 major 或 natural minor。

语义规则：

1. 使用**音高拼写**确定音级：字母（C..B）相对主音字母的音级位置，而不是半音距离。
2. 音级内的半音变化量相对该音级的调内音计算。
3. 移动后，半音变化量原样保留，并重新计算拼写与八度。
4. 不做等音重写；不把非调内音吸附到最近的调内音。

因此 F minor 下：

```text
B3  +1 → C#4   B♮ 是升高的四级，移动一级得到升高的五级
E4  +1 → F#4   E♮ 是升高的七级，移动一级得到升高的主音
C#4 +2 → E4    C# 是升高的五级，移动两级得到升高的六级（E♮）
F4  -2 → Db4
```

输出保证：

- 输出音符数、偏移、时值、力度、origin、span 与输入一致。
- 同一个音高在同一上下文下产生确定性结果。

失败行为：

- 结果超出 MIDI 范围时抛 `pitch` / `harmony` 错误。
- 非法 Key 名抛 `key` 错误。

### 8.2 Harmonize

前置条件：

- `degreeOffsets` 非空，元素为非零整数音级偏移。
- `velocity` 在 1..127。
- 上下文 Key 合法。

输出保证：

- 每个输入音保留原音，并按偏移顺序追加和弦音。
- 生成音与原音同起点、同时值，力度为参数值。
- 原音 origin 不变；生成音 origin = harmonize 节点 id。
- 输出 span 与输入一致，offset 排序稳定。

失败行为：

- 空偏移、非法力度、越界音高抛错。
- 不保证和声功能、声部进行、避免平行五八度或声部交叉。

### 8.3 Syncopate

前置条件：

- Meter 合法（正分子、2 的幂分母）。
- displacement 为有理数，可正可负。

语义：

- 当且仅当 `offset / beatLength` 为整数时，把该音后移 displacement。
- 位移后为负则夹到 0。
- 输出 span = max(输入 span, 所有新结束位置)。

输出保证：

- 音高、时值、力度、origin 不变；只有 offset 与 span 可能改变。
- 非拍点音符完全不变。

失败行为：

- 非法拍号由 Meter 上下文校验抛错。
- 不建模重音、切分张力、风格或声部关系。

## 9. 时序曲线算子

Temporal IR 的 Constant / Linear / Step / Smoothstep / Keyframes / Shift / Stretch /
Reverse / Repeat 语义、定义域、边界与序列化格式见 `docs/temporal-spec.md`。这些算子
与音乐算子一样是纯值图，不使用不可序列化的函数对象。
