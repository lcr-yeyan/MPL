# MPL 声部关系与优化契约（V0.3.0 Final）

状态：实现规范。对应代码：`mpl/music/Chord.hpp`（Voicing/voiceLead）、
`mpl/music/Analysis.hpp`、`mpl/music/Instrument.hpp`。

## 1. 三类接口

### Analyze（只报告事实）

| 接口 | 结果 |
| --- | --- |
| `interval(a, b)` | `b.midi - a.midi` |
| `pitchClass(p)` | 0..11 |
| `pitchRange(list)` | 最高-最低 MIDI |
| `fragmentRange(f)` | Fragment 音域 |
| `chordContains(c, p)` | 音高类别是否属于和弦 |
| `commonTones(a, b)` | 共同 pitch-class 音高（取 a 的拼写/八度） |
| `voiceDistance(a, b)` | 同序号声部移动距离之和 |
| `voiceCrossings(v)` | 声部交叉对数 |
| `noteOverlap(a, b)` | 两 Fragment 中持续区间重叠的音符对数量 |

### Validate（显式规则）

| 接口 | 语义 |
| --- | --- |
| `voicingStatus(v)` | 搜索/硬约束状态 |
| `voicingComplete(v)` | 是否覆盖全部和弦音 |
| `parallelPerfects(lower, upper)` | 连续平行纯五/八度数量；只有显式调用才构成违规 |

**默认不违规**：非和弦音、平行运动、声部交叉、重复音、省略音都不会自动报错。只有当程序
显式查询并在 `if`/函数中据此行动时，才成为作品约束。

### Construct（生成）

| 接口 | 语义 |
| --- | --- |
| `voicing(chord, low, high, voices, inversion, maxSpacing)` | 确定性和弦排列 |
| `voiceLead(previous, chord, low, high, maxMove, maxSpacing, allowCrossing)` | 有界声部连接 |
| `zip` / `cycle` | Rhythm × Pitch 组合 |

## 2. Voicing 构造契约

输入：和弦、音域 `[low, high]`、声部数 `voices ∈ [1,8]`、转位、最大相邻间距。

硬约束：音域、声部数、和弦音、相邻间距、低音转位类。

规则：

1. 低音 = 转位对应 pitch class 在音域内的最低音；
2. 从低音按转位顺序循环取和弦音向上构造；
3. 多于和弦音数时循环重复；
4. 少于和弦音数时省略转位顺序末尾的音；
5. 失败返回 `ProvablyUnsatisfiable`（音域/间距无法容纳），不返回近似音高。

## 3. Voice-leading 搜索契约

### 3.1 输入

- `previous`：上一排列的低到高 pitch 列表（声部数 = 列表长度）；
- `chord`：目标和弦；
- `low`/`high`：音域；
- `maxMove`：每声部最大移动半音数（目标，硬约束）；
- `maxSpacing`：相邻声部最大间距；
- `allowCrossing`：是否允许声部交叉；
- 可选 `budget`：候选节点预算（默认 20000）。

### 3.2 硬约束

- 声部数不变；
- 每个音高在音域内且属于目标和弦；
- `allowCrossing=false` 时严格低到高，且相邻间距 ≤ maxSpacing；
- 每声部移动 ≤ `maxMove` 才算严格满足；`maxMove+1` 半音为容差。

### 3.3 优化目标

最小化总移动距离：

```text
sum |next[i].midi - previous[i].midi|
```

平局：低到高 pitch 列表字典序更小者优先。

### 3.4 确定性

- 每声部候选按“移动距离、MIDI”排序；
- DFS 顺序固定；
- 预算计数固定；
- 相同输入与预算必然产生相同结果。

### 3.5 状态

| 状态 | 含义 |
| --- | --- |
| `strictly-satisfied` | 存在满足全部硬约束且每声部移动 ≤ maxMove 的候选 |
| `satisfied-within-tolerance` | 存在候选，但最差声部移动在 maxMove+1 内 |
| `unsatisfied` | 有候选但未达硬约束/容差 |
| `backend-capability` | 请求超出后端/乐器能力（预留给音频/指法后端） |
| `budget-exhausted` | 搜索预算耗尽；**不**声明无解 |
| `provably-unsatisfiable` | 完整枚举证明无候选（如音域内无和弦音） |

`budget-exhausted` 与 `provably-unsatisfiable` 必须分开报告。MPL 不把预算耗尽说成
数学意义上的无解。

## 4. 声部交叉

- `voiceCrossings(notes)` 统计任意两声部顺序反转的对数；
- `allowCrossing=false` 时搜索拒绝任何交叉；
- `allowCrossing=true` 时不强制顺序，允许交叉；间距约束仍由调用方决定；
- 交叉本身不是错误，只有显式约束才构成违规。

## 5. 共同音、重复音、省略音

- 共同音由 `commonTones` 分析；搜索不强制保留共同音；
- 重复音由 voicing 的循环规则产生；
- 省略音由 voices 少于和弦音数产生；
- `complete` 是显式信息，不是默认错误；
- 若需要“必须完整”约束，调用方检查 `voicingComplete(v)`。

## 6. 平行运动

`parallelPerfects(lower, upper)`：

- 两声部长度相等且至少 2 个音；
- 连续两个音程都是纯五度（7）或八度/同度（0）；
- 两个声部都移动且方向相同；
- 返回计数。

该函数不会自动调用；只有程序显式使用时才构成验证规则。

## 7. 乐器能力

`InstrumentCapability`：

- 音域 `[lowestMidi, highestMidi]`；
- 最大复音数；
- 开放弦与品数（可选）；
- `positions(pitch)` 返回弦/品位置；
- `fitsInstrument` 检查音域与复音数。

标准吉他：E2 A2 D3 G3 B3 E4，24 品，6 复音。

乐器约束不进入 `Pitch`/`Chord` 的通用语义；它只作为显式验证/构造输入。

## 8. 与 Rhythm 的关系

- 声部关系处理 pitch/interval；Rhythm 关系处理起音集合；
- 两者可以组合（例如先求起音交集，再为交集音高配和弦音）；
- 不引入通用跨声部约束求解器。

## 9. 范围外

- 完整指法求解器；
- 声部进行风格规则库；
- 自动平行五八度避免；
- 通用约束优化/搜索框架；
- 微分音声部连接。
