# MPL 音高与和声语义规范（V0.3.0 Final）

状态：实现规范。对应代码：`mpl/music/Pitch.hpp`、`Key.hpp`、`Chord.hpp`、
`HarmonyTimeline.hpp`、`Harmony.hpp`、`Analysis.hpp`。

## 1. 边界

- MPL 定义可执行的音高/和声计算，不定义普适审美；
- MIDI 0..127 是编码边界，不是音乐理论边界；
- 不实现完整微分音或任意调律；本节所有音高以 12 平均律半音为运行表示；
- 和弦功能、风格与“好听”不进入默认判定。

## 2. 类型

### 2.1 Pitch

带拼写的音高：字母、变音记号、八度、派生 MIDI 编号。拼写保留，等音不自动重写。

### 2.2 PitchClass

`PitchClass { int value ∈ [0,11] }`：

- `fromPitch` 取 `midi % 12`；
- `parse("F#")` / `parse("Bb")` 精确解析；
- 显示使用固定拼写表（C, C#, D, Eb, E, F, F#, G, Ab, A, Bb, B）；
- 与 `Pitch` 不同：没有八度，也没有拼写身份。

### 2.3 Interval / ScaleDegree

- `Interval`：半音距离，可负；
- `ScaleDegree`：调内音级步数，用于 `diatonicTranspose`；
- 二者不互换；`diatonicTranspose` 保留输入音级的半音变化。

### 2.4 Key

主音拼写 + `major` / `natural minor`。RC 不扩展调式系统。

### 2.5 PitchClassSet

```cpp
struct PitchClassSet { std::vector<int> values; }; // sorted, unique, [0,11]
```

操作：`contains`、`containsAll`、`intersection`、`name`。用于和弦音与共同音查询。

### 2.6 ChordSymbol

```cpp
struct ChordSymbol {
    int rootPitchClass;          // -1 = none
    ChordQuality quality;
    std::vector<int> intervals;  // semitones above root, starts at 0
    int bassPitchClass;          // -1 = root position
    std::string label;
};
```

- `parse` 支持：大三/小三/减/增、sus2/sus4、5、6、m6、7、maj7、m7、m7b5、dim7、mMaj7、
  9/11/13 扩展、b5/#5/b9/#9/#11/b13 变化音、`/Bass` 转位低音；
- `pitchClassSet()` 返回去重后的和弦音；
- `contains(pc)` 按 pitch class 判断；
- `inversion()` 返回低音是第几个和弦音（0=根音）；
- `none()` 表示无效/空和弦（用于和声查询 gap）。

### 2.7 Voicing

```cpp
struct Voicing {
    std::vector<Pitch> notes;    // low to high
    VoicingStatus status;
    int totalMovement;
    int crossings;
    bool complete;
    std::string detail;
};
```

`Voicing` 是具体八度/排列结果，不是和弦身份。

### 2.8 HarmonicFunction

保留枚举接口（Tonic/Subdominant/Dominant/Other）；RC 不自动推断功能，也不把功能判断
作为默认错误。

## 3. 和弦构成规则

### 3.1 基础和弦

| 后缀 | 音程（半音） |
| --- | --- |
| 空 / maj | 0,4,7 |
| m / min | 0,3,7 |
| dim / o | 0,3,6 |
| aug / + | 0,4,8 |
| sus2 | 0,2,7 |
| sus4 | 0,5,7 |
| 5 | 0,7 |
| 6 | 0,4,7,9 |
| m6 | 0,3,7,9 |
| 7 | 0,4,7,10 |
| maj7 / M7 | 0,4,7,11 |
| m7 | 0,3,7,10 |
| m7b5 / ø7 | 0,3,6,10 |
| dim7 | 0,3,6,9 |
| mMaj7 | 0,3,7,11 |

### 3.2 扩展音

- 9 加 14；11 加 17；13 加 21；
- 9/11/13 默认包含相应的七音（属七、大七或小七，取决于前缀）；
- 大前缀 `maj`/`M` 表示大七；`m`/`min`/`-` 表示小七；无前缀表示属七。

### 3.3 变化音

`b5`→6（替换 7），`#5`→8，`b9`→13，`#9`→15，`#11`→18，`b13`→20。
若扩展音未显式出现，先补自然音再替换（例如 A7b9 会加入 13）。

### 3.4 转位

`/Bass` 设置 `bassPitchClass`。`inversion()` 返回低音在和弦音列表中的序号；不在和弦音内
的低音仍可解析，但 `inversion()` 返回 0，voicing 可能报不可满足。

### 3.5 重复音、省略音

`buildVoicing` 的明确规则：

- 低音：请求转位对应的最低和弦音（在音域内）；
- 从低音起按转位顺序循环取和弦音，向上构造；
- `voices` 多于和弦音数：循环重复低音、五音、根音（从转位顺序）；
- `voices` 少于和弦音数：只取转位顺序的前 `voices` 个音，因此省略上方扩展音；
- `complete` 标记是否覆盖全部和弦 pitch class；
- 省略/重复不是错误，但 `complete=false` 是显式信息。

### 3.6 非调内音

- `ChordSymbol` 可以包含任何变化音；
- 没有“必须调内”的默认规则；
- 调内约束若需要，由用户显式调用分析接口或函数比较。

## 4. Harmony Timeline

```cpp
struct HarmonyEvent {
    Rational start;
    Rational duration;
    ChordSymbol chord;
    ObjectId origin;
};
```

规则：

- 事件覆盖 `[start, start+duration)`；
- 允许重叠、空白；
- `logicalLength >= max(start+duration)`；
- `eventAt(position)`：覆盖该时刻的事件中 start 最大者；同 start 取后声明者；
- `eventsAt(position)`：全部覆盖事件，按 start 排序；
- 边界右连续；gap 返回 `ChordSymbol::none()`；
- 事件必须 start ≥ 0、duration > 0、chord 有效、在逻辑长度内。

语言：

```text
let H = harmony span 10 whole {
  "Dm9" @ 0 whole for 7/4 whole,
  "G7b9" @ 7/4 whole for 7/4 whole
};
```

- 位置/时长使用当前 MetricScope 解析；`bars`/`beats` 在 Section 中按该 Section 的 MeterMap；
- 和声时间线自身不依赖拍号；拍号只影响 bars 字面量如何折叠；
- 跨小节与变拍号由绝对位置表达。

## 5. Music IR 与溯源

- `harmony-literal` 节点保存 HarmonyTimeline；
- `harmony-query` 节点保存查询位置，输入为 harmony 节点；
- 求值缓存 `harmonyValue` / `chordValue`；
- 查询结果进入 Trace 的 `chordValue`；
- 事件 origin 指向创建时间线的节点；
- `chordAt`/`chordAtBar` 均创建查询节点。

## 6. 音高/和声组合

- `chordAt(h, at)` / `chordAtBar(h, bar)` 选择和声；
- `voicing(chord, low, high, voices, inversion, maxSpacing)` 生成排列；
- `voiceLead(previous, chord, low, high, maxMove, maxSpacing, allowCrossing)` 生成连接；
- `voicingNotes(v)` / `voicingStatus(v)` / `voicingMovement(v)` / `voicingCrossings(v)` /
  `voicingComplete(v)` 查询结果；
- `zip` / `cycle` 把 Rhythm 与具体音高组合；
- 和声不直接生成音符；必须有显式音高策略。

## 7. 诊断

| 代码 | 条件 |
| --- | --- |
| `type-chord` | 和弦符号解析失败 |
| `type-harmony` | 和声字面量条目非法 |
| `type-harmony-empty` | 空和声字面量/零长度 |
| `harmony-timeline` | 事件越界/非法 duration/chord |
| `voicing` | voicing 参数非法 |
| `voice-leading` | voiceLead 参数非法 |
| `type-pitch` | 音高解析失败 |

## 8. 范围外

- 完整微分音/任意调律；
- 自动功能和声分析；
- 风格化和弦选择；
- 通用和弦/声部约束求解；
- 完整指法求解器。
