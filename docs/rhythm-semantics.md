# MPL 节奏语义规范（V0.3.0-beta）

状态：实现规范。对应代码 `include/mpl/music/Rhythm.hpp`、`src/music/Rhythm.cpp`。

本规范定义独立于音高、乐器与 MIDI 通道的 Rhythm 值及其代数。它只定义精确计算与可追溯性，不定义“好听”“复杂”或风格评价。

## 1. Rhythm 值

```cpp
struct RhythmEvent {
    Rational onset;      // 相对 Rhythm 原点的起音位置，可负
    Rational duration;   // 正时值
    int accent;          // 0..8，结构重音属性
    bool rest;           // true = 显式休止
    ObjectId origin;     // 生成该事件的 Music IR 对象
};

class Rhythm {
    std::vector<RhythmEvent> events;  // 按 onset 稳定排序
    Rational logicalLength;           // 逻辑长度/周期
};
```

语义：

- `logicalLength` 是 repeat/rotate/overlay/关系运算使用的周期；
- 事件可短于周期（逻辑留白），也可长于周期（持续尾音）；组合时 Fragment span 取周期；
- `rest` 事件保留显式休止，但不产生音符，也不消耗音高；
- `accent` 是结构属性，不是力度；力度由 Rhythm × Pitch 的显式映射产生；
- 事件重叠合法，`overlay` 不合并重叠或重复起音；
- 同一个 Rhythm 可被多个 Fragment 生成节点共享，但仍保持独立调用/演奏实例身份。

不变式：

- `logicalLength >= 0`；零长度 Rhythm 必须无事件；
- 每个事件 `duration > 0`；
- `0 <= accent <= 8`；
- 事件数不超过 `kMaxRhythmEvents = 4096`；
- 事件按 `onset` 非降序存储（相等时保持输入顺序）。

## 2. 字面量

```text
rhythm [span <duration>] {
  hit <duration> [accent <int>],
  rest <duration>,
  ...
}
```

语义：

- 条目顺序布局，`onset` 从 0 开始累加；
- 逻辑长度 = 显式 span（若给出）或内容长度总和；
- span 必须为正，且不得短于内容末端；
- 休止条目保留在事件列表中；
- 字面量本身是 Music IR 的 `rhythm-literal` 节点，事件 `origin` 指向该节点。

示例：

```text
let R3 = rhythm {
  hit 1/8 accent 2,
  hit 1/8,
  hit 1/16,
  rest 1/16
};
// logicalLength = 3/8, attacks = 3, rest at 5/16
```

## 3. 代数

所有算子返回新的 Rhythm；输入不被修改。

### 3.1 sequence(parts...)

- 依次拼接；第 i 部分整体平移前 i-1 部分逻辑长度之和；
- 结果逻辑长度 = 各部分逻辑长度之和；
- 至少一个输入。

### 3.2 overlay(parts...)

- 所有部分从 0 对齐；
- 结果逻辑长度 = 最大逻辑长度；
- 事件全部保留，允许同 onset 多事件；
- 至少一个输入。

### 3.3 repeat(rhythm, count)

- `1 <= count <= kMaxRhythmRepeat = 256`；
- 第 i 份平移 `i * logicalLength`；
- 结果逻辑长度 = `count * logicalLength`；
- 结果事件数受 `kMaxRhythmEvents` 限制。

### 3.4 rotate(rhythm, amount)

- 周期 = `logicalLength`，必须为正；
- 相位 = `amount mod logicalLength`，负 amount 规范到 `[0, L)`；
- 事件 onset 循环平移；
- **跨周期边界的事件被精确切分**：例如 `[1/4, 1/4+3/8)` 在 3/8 周期内旋转后切成 `[1/4, 3/8)` 与 `[0, 1/4)`；
- 单个事件最多切分 256 段，超出报 `rhythm-event-limit`；
- 结果逻辑长度不变。

### 3.5 shift(rhythm, amount)

- 线性平移所有 onset，可正可负；
- **不环绕**；负 amount 可产生负 onset（pickup）；
- 逻辑长度不变；
- 负 onset 的 Rhythm 可以继续做代数，但不能直接与音高组合（见 §5）。

### 3.6 stretch(rhythm, factor)

- `factor > 0`；
- onset、duration、logicalLength 同乘 factor；
- 结果为精确有理数。

### 3.7 subdivide(rhythm, parts)

- `1 <= parts <= kMaxRhythmSubdivision = 64`；
- 每个事件（含休止）切成 parts 个等长片段；
- onset 依次为 `onset + k * duration / parts`；
- **accent 原样复制**：这是时值操作，不是重音规则；
- 逻辑长度不变；
- 事件数受上限限制。

### 3.8 accent(rhythm, levels)

- `levels` 为单值或列表；列表循环应用于**起音事件**（按 onset 顺序，休止不计数）；
- 每个 level 在 `[0, 8]`；
- 休止事件不变；
- 逻辑长度不变。

### 3.9 realign(rhythm, targetPeriod)

- 要求 `targetPeriod` 是 `logicalLength` 的正整数倍；
- 等价于 `repeat(rhythm, target / logicalLength)`；
- 用于把不同周期精确对齐到共同边界；
- 结果逻辑长度 = `targetPeriod`；
- 非整数倍、零或负目标报 `rhythm-realign`。

### 3.10 quantize(rhythm, grid)

- `grid > 0`；
- 每个 onset 吸附到最近的 grid 倍数，**平局向正无穷**；
- duration 与 logicalLength 不变；
- 吸附后产生相同 onset 的事件保留，不自动合并；
- 负 onset 使用同样的 floor/round 规则。

## 4. 周期与共同边界

`rhythmCommonPeriod(a, b)` 返回两个正周期的最小正公倍数：

```text
lcm(a/b, c/d) = lcm(a, c) / gcd(b, d)   （各自已约分）
```

例：

```text
lcm(3/8, 7/8) = 21/8     // 7 个 3/8 = 3 个 7/8
lcm(3/8, 1/2) = 3/2      // 4 个 3/8 = 3 个 1/2
lcm(1/2, 3/4) = 3/2
```

容量契约：

- 任何输入分子/分母超过 `kMusicalRationalLimit = 1e9` 时返回 `nullopt`；
- 结果超过上限时返回 `nullopt`；
- 关系算子遇到不可表示的共同周期抛 `rhythm-period-overflow`，不展开无界事件。

## 5. Rhythm 与 Pitch 的组合

### 5.1 zip(rhythm, pitches, baseVelocity)

严格映射：

- 起音数必须等于音高数，否则诊断 `rhythm-pitch-count`；
- 休止不消耗音高；
- 每个 note：`offset = event.onset`，`duration = event.duration`；
- 负 onset 不能放到 Fragment 时间轴，诊断 `rhythm-pitch`；
- 力度：`clamp(baseVelocity + accent * 8, 1, 127)`；这是把结构重音映射为演奏力度的显式规则；
- Fragment span = `rhythm.logicalLength`，保留尾部逻辑留白；
- `note.origin` = `rhythm-combine` 节点；`note.rhythmOrigin` = 事件 origin；`note.rhythmEventIndex` = 事件在 Rhythm 中的索引。

### 5.2 cycle(rhythm, pitches, baseVelocity)

循环填充：

- 音高序列非空；
- 第 i 个起音使用 `pitches[i mod count]`；
- 其余规则同 zip；
- 允许音高数少于、等于或多于起音数（多余音高被忽略）。

### 5.3 明确规则

- 不提供隐式“音高不够就补主音”或“音高多了就丢弃”的默认行为：必须显式选择 zip 或 cycle；
- 休止永远不消耗音高；
- 组合结果不依赖音高、乐器或 MIDI 通道的 Rhythm 本体保持不变。

## 6. 跨声部起音关系

关系算子处理的是**起音集合**，不是完整持续区间。两个声部中一个长音覆盖另一个声部的起音点时，长音不会“吸收”该起音。

所有关系使用两个 Rhythm 的 `rhythmCommonPeriod` 作为结果逻辑长度。

### 6.0 onsetSet(rhythm, unit)

- `unit > 0`；
- 每个起音保留 onset 与 accent，duration 改为 `unit`；
- 休止事件被丢弃（起音集合没有休止）；
- 逻辑长度不变；
- 用于把带持续时值的节奏显式归约为纯起音集合，再参与交/并/差。

### 6.1 精确坐标（无 grid）

- 以精确有理 onset 为键；
- 同一 onset 的多个事件取输入顺序中的第一个；
- `intersect(a,b)`：两者共有的 onset；元数据（duration/accent/origin）取 a；
- `union(a,b)`：任一方的 onset；a 优先；
- `difference(a,b)`：a 有而 b 没有的 onset；取 a 元数据。

### 6.2 网格坐标（有 grid）

- `grid > 0`；
- onset 先按 §3.10 的规则吸附到最近 grid 点，再以 grid 点为键；
- 结果事件 onset = grid 点，duration = grid；
- 元数据取 a（若 a 无该点则取 b）；
- grid 只影响匹配与结果时值，不改变原始 Rhythm。

### 6.3 complement(rhythm, span, grid)

- `span > 0`，`grid > 0`，`span` 必须是 `grid` 的整数倍；
- 在 `[0, span)` 的所有 grid 点中，排除 rhythm 的精确 onset；
- 结果 duration = grid，accent = 0；
- 结果逻辑长度 = span；
- grid 点数受 `kMaxRhythmEvents` 限制。

### 6.4 accentFollow(source, target, [grid])

- 以 source 的起音 accent 为来源、target 的 onset 为目标；
- 精确模式或 grid 模式匹配；
- 匹配到则 target accent = source accent，否则 0；
- 保留 target 的 onset/duration/rest 与逻辑长度。

### 6.5 与节拍层级的关系

`metricAccent(rhythm)` 使用当前 MetricScope（MeterMap + anchor）为每个起音设置：

```text
accent = metricWeight(anchor + onset).level   // 3/2/1/0
```

`metricWeight(offset)` 在语言层返回同一层级。节拍分组只提供结构权重；是否使用、如何使用由显式算子决定。

## 7. 身份、缓存与溯源

- Rhythm 是 Music IR 值图中的一个对象；与 Fragment 共用 `ObjectId` 空间；
- `isRhythmObject(id)` 区分值类型；
- 节点类型：`rhythm-literal`、`rhythm-sequence`、`rhythm-overlay`、`rhythm-repeat`、`rhythm-rotate`、`rhythm-shift`、`rhythm-stretch`、`rhythm-subdivide`、`rhythm-accent`、`rhythm-realign`、`rhythm-quantize`、`rhythm-accent-by-meter`、`rhythm-intersect`、`rhythm-union`、`rhythm-difference`、`rhythm-complement`、`rhythm-accent-follow`、`rhythm-combine`；
- `evaluateRhythm(id)` 缓存 Rhythm 值；`invalidate` 同时清除 Fragment 与 Rhythm 缓存；
- 合成事件（如 complement 网格点）若没有 origin，由节点求值时补为节点 id；
- `rhythm-combine` 的 note 记录事件级索引，Trace 可逐音定位到 Rhythm 事件。

## 8. 容量与失败

| 限制 | 值 | 诊断 |
| --- | --- | --- |
| 事件数 | 4096 | `rhythm-event-limit` |
| repeat | 256 | `rhythm` |
| subdivide | 64 | `rhythm` |
| 单事件旋转切分 | 256 段 | `rhythm-event-limit` |
| 有理数分子/分母 | 1e9 | `rhythm-period-overflow` |
| 负 onset 与音高组合 | 禁止 | `rhythm-pitch` |
| zip 数量不匹配 | 禁止 | `rhythm-pitch-count` |
| 非整数倍 realign | 禁止 | `rhythm-realign` |
| 空 accent levels | 禁止 | `rhythm` |

所有失败返回诊断或抛 `mpl::Error`（由前端转诊断），不进行无界展开或静默近似。

## 9. 范围外

- 概率节奏、风格律动、人类化（humanize）模型；
- 以节奏复杂度评分代替音乐评价；
- 自动声部进行 / 通用跨声部约束求解；
- 音频速率节奏调制；
- 任意递归节奏生成。
