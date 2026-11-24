# MPL 语义模型（Core v0.0.3）

## 1. 两级 IR

| 属性 | Music IR | Production IR |
| --- | --- | --- |
| 关注点 | 创作逻辑 | 工程执行 |
| 结构 | 类型化依赖 DAG | 时间线 + 轨道/片段 |
| 动机与变奏关系 | 保留 | 展开为事件 |
| 和声关系 | 保留（Key 上下文） | 间接保留为 `sourceObject` |
| 时间 | 全音符有理数 | 全音符有理数（绝对位置） |
| 后端属性 | 无 | 无 |

Production IR 只描述“有哪些轨道、哪些片段、从哪开始、包含哪些音、绑定什么音源”。MIDI 通道、GM Program、PPQ 属于 `MidiBackend`。

## 2. Music IR 节点

节点类型：

| 节点 | 输入 | 上下文依赖 | 值 |
| --- | --- | --- | --- |
| `literal` | 无 | 无 | Fragment |
| `transpose` | 1 | 无 | Fragment |
| `reverse` | 1 | 无 | Fragment |
| `stretch` | 1 | 无 | Fragment |
| `repeat` | 1 | 无 | Fragment |
| `sequence` | N | 无 | Fragment |
| `parallel` | N | 无 | Fragment |
| `diatonic-transpose` | 1 | key | Fragment |
| `harmonize` | 1 | key | Fragment（含生成音） |
| `syncopate` | 1 | meter | Fragment |

图的不变量：

1. 算子只能引用已存在的对象 id，因此图天然无环。
2. 对象一旦创建，其算子参数不可变；唯一允许的内容修改是 `updateLiteral`。
3. `value` 是求值缓存，不是语义的一部分；序列化时可选输出。

## 3. 生成依赖与上下文依赖

- **生成依赖**：`inputs`。由 `dependents()` 反向遍历，用于内容修改后的失效传播。
- **上下文依赖**：节点类型声明的 `key` / `meter` 读取。修改上下文只失效声明了该依赖的节点及其下游。

这两类依赖分开建模的原因是：移调不关心调号，而 `diatonicTranspose` 关心；`syncopate` 关心拍号，而 `reverse` 不关心。

## 4. 增量求值与一致性

- `evaluate(id)` 命中缓存返回旧值，否则递归求值输入并写入缓存。
- 统计：`evaluationCount()`（实际求值次数）与 `cacheHitCount()`（缓存命中次数）。
- `updateLiteral(id, ...)`：替换字面量内容并失效该对象及全部下游。
- `setKey` / `setMeter`：只失效对应上下文敏感的节点及下游。

一致性由测试锁定：同一序列修改后做增量求值，与从零重建等价图并全量求值，`MusicIR::toJson()` 必须逐字节一致。测试同时验证未被影响的独立对象只产生缓存命中。

当前失效算法是线性扫描 + 下游 BFS，适合原型规模；不是最终性能设计。

## 5. 逐音符溯源

`Note::origin` 记录引入该音符的 Music IR 对象：

- `literal` 创建时，所有音符 origin = 字面量 id；
- `transpose` / `reverse` / `stretch` / `repeat` / `sequence` / `parallel` 原样复制 origin；
- `harmonize`：输入音保留 origin，新生成的和弦音 origin = harmonize 节点 id；
- `syncopate`：保留 origin（只改时间）。

`Compilation::toTraceJson()` 输出：

```json
{
  "objects": [ { "id": 15, "kind": "harmonize", "label": "Pad Voicing", ... } ],
  "provenance": [
    {
      "track": "Pad", "clip": "Drop A", "sourceObject": 15,
      "derivation": [15, 14, 13, 9, 10, 11, 12],
      "notes": [
        { "pitch": "F2", "atWholeNotes": "4", "origin": 9, "originLabel": "i" },
        { "pitch": "Ab2", "atWholeNotes": "4", "origin": 15, "originLabel": "Pad Voicing" }
      ]
    }
  ]
}
```

`derivation` 是片段根节点到所有输入的拓扑前序链，因此单条音符记录即可回答“它由哪个算子生成、该算子依赖哪些对象”。

## 6. 降低到 Production IR

- 每个 `TrackSpec` 变成 `TrackIR`：名称、符号音源、角色、资产槽与绑定。
- 每个 `ClipSpec` 求值一次，复制音符到 `ClipIR`，记录 `sourceObject` / `sourceLabel`。
- 多个轨道引用同一对象时，值被共享复用，但每个 clip 独立保存音符快照（当前实现），保证生产层可独立修改。
- 轨道起止位置由 clip 的 `start + content.duration()` 决定。

## 7. 资产模型

```text
InstrumentRole  melodic | bass | harmony | percussion | texture
AssetSlot       name, kind(instrument|sample), preferredAsset
AssetBinding    slot, assetId, provider   // provider="symbolic" 表示仅命名绑定
```

- 角色是编排语义，不是音源。
- 槽位是“这里需要一个声音”，允许一个轨道未来有多个槽（主体、叠层、采样）。
- 绑定可以未解析；`validate-assets` 只检查槽位/绑定引用与重复，不强制解析。
- MIDI 后端用符号音源名选择 GM Program；未来 SoundBoxer 后端消费 provider/assetId。

## 8. 已知局限

1. 只有一个 `Note::origin`，不记录完整节点路径；完整路径通过 clip 的 `derivation` 拼接得到。
2. Production IR 的 clip 内容是快照，不是对 Music IR 的引用；修改图后必须重新编译整个项目。
3. 上下文只有 key 与 meter，尚无速度曲线、演奏法、音源能力等。
4. 和声算子只做音级叠加，不理解和弦功能、声部进行规则。
5. 失效传播为全图扫描，未做增量索引。
6. `Rational` 使用 int64；极端分子/分母会抛错而不是降精度。
7. MIDI 后端把多个 percussion 轨道都放在通道 10，未做鼓组音符映射。

## 9. V0.0.3：Temporal IR、实例与自动化

### 9.1 三个图

```text
MusicIR     创作定义（动机、变奏、和声）
TemporalIR  时变属性定义（曲线图，独立求值）
ProductionIR 播放实例（轨道、clip、表情、绑定）
```

Temporal IR 不依赖 Music IR；修改曲线不会失效任何音乐对象。反过来，修改音乐对象也不会失效曲线。两者只在 lowering/导出时通过 id 关联。

### 9.2 曲线节点与缓存

| 节点 | 输入 | extent |
| --- | --- | --- |
| constant / linear / step / smoothstep / keyframes | 无 | 创建时指定 |
| shift | 1 | 与输入相同（域外保持边界值） |
| stretch | 1 | 输入 extent × factor |
| reverse | 1 | 与输入相同 |
| repeat | 1 | 输入 extent × count |

- 求值结果为 `Curve{domain, extent, segments}`，段有序、非重叠、左闭右开。
- 节点缓存 `value`；`updateConstant` / `updateLinear` / `updateKeyframes` 只失效该节点及其下游。
- `evaluateAll` 按 id 升序求值，id 即拓扑序。
- 统计 `evaluationCount` / `cacheHitCount` 用于验证增量行为。

### 9.3 实例与身份

- 定义：Music IR 对象，`ObjectId` 稳定。
- 播放：Production IR 的 clip，`InstanceId` 按 lowering 顺序分配；同一对象可被多个 clip 引用。
- 音符：`NoteInstanceId = (InstanceId, noteIndex)`；trace 直接输出字符串 `"instance:index"`。
- 参数：`ParameterId` 使用名称而非索引，添加/重排轨道不会改变已有参数 id。
- 实例编号在当前项目 lowering 顺序下稳定；改变轨道/片段顺序会重新编号，这是当前实现的限制。

### 9.4 表情与自动化

- `PerformanceExpression` 属于 clip 实例，包含 velocity / timing / gate / pan 四条 Relative 曲线。
- 表情曲线必须是 extent = 1 的 Relative 曲线，并在参数范围内；否则编译诊断。
- `AutomationBinding` 属于 Production IR，目标为 track/clip/master 参数。曲线在 musical 或 relative 域求值。
- 组合：Replace 提供基值；Add 求和；Multiply 求积。多个 Replace 冲突是错误。
- MIDI 后端把 track/musical 绑定采样为 CC；其他作用域/域产生明确诊断。

### 9.5 Trace v3

新增：

```json
"context": {...},
"music": {"objects": 9, "evaluated": 9, "evaluations": 9, "cacheHits": 49},
"temporal": {"objects": 9, "curves": [...]},
"automation": [{"parameter": "track:Piano:volume", "mode": "replace", ...}],
"expressions": [{"track": "Piano", "clip": "A Humanized", "instanceId": 2,
                 "velocity": 3, "timing": 2, "gate": 4, "pan": 5}],
"provenance": [{"instanceId": 2, "notes": [{"origin": 0,
                "originChain": [0], "noteInstance": "2:0"}]}]
```

`originChain` 是从 clip 根对象到音符直接来源的确定性首路径；`derivation` 列出全部依赖对象。多源 clip 通过不同 origin 区分（例如原音 vs harmonize 生成音）。

### 9.6 已知局限（V0.0.3）

1. Physical / Sample 域曲线只定义了类型边界，未实现求值。
2. shift 超出 extent 的部分按边界值保持；step 的域边界采用右连续约定。
3. 表情是 clip 级曲线，同 clip 内音符共享；没有逐音符独立自动化。
4. MIDI CC 采样固定为 1/16 全音符（最多 2048 点），不是采样级精度。
5. 仅支持单一速度/拍号事件；没有 tempo map 曲线。
6. 没有实时调制、MPE、微分音、音色/空间参数导出。
7. `originChain` 只给首路径，不枚举所有依赖路径。
8. Rational 仍为 int64 有界精确值。
