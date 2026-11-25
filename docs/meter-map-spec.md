# MPL Meter Map 规范（V0.3.0-beta）

状态：实现规范。对应代码 `include/mpl/core/MeterMap.hpp`、`src/core/MeterMap.cpp`。

本规范定义 MPL 在变拍号环境下的度量语义。它只回答“时间怎样被精确表示与换算”，不定义情绪、风格或审美评价。

## 1. 五个必须区分的概念

| 概念 | 类型 | 含义 |
| --- | --- | --- |
| Absolute Musical Time | `Position` / `Rational` | 从作品起点计的全音符有理数；唯一绝对时间轴 |
| Time Signature | `Meter` | 分子/分母，例如 7/8 |
| Metric Grouping | `MetricGrouping` | 小节内拍的分组，例如 2+2+3 |
| Metric Position | `MetricPosition` | 小节编号 + 小节内拍偏移（以分母单位计） |
| Metric Duration | `MetricDuration` | “N 小节 / N 拍”的度量长度，需锚点才能折叠为绝对时值 |

`MetricPosition` 不是新的时间轴，只是同一 `Position` 的坐标表示。MPL 不增加第五种 `TimeDomain`。

## 2. 数据结构

### 2.1 Meter

```cpp
struct Meter {
    int numerator;
    int denominator;
    Rational barLength() const;   // numerator / denominator
    Rational beatLength() const;  // 1 / denominator
};
```

约束：`numerator >= 1`，`denominator` 为 2 的幂。

### 2.2 MetricGrouping

```cpp
struct MetricGrouping {
    std::vector<int> groups;      // 以分母单位计，例如 {2,2,3}
    bool explicitGrouping;
};
```

约束：

- 每个 group 为正整数；
- group 之和等于 `meter.numerator`；
- 空 groups 表示“每拍一组”的默认分组，写入 `MeterMap` 时规范化为 `numerator` 个 1；
- grouping 不改变小节长度，也不直接改变演奏力度。

### 2.3 MeterMapEvent

```cpp
struct MeterMapEvent {
    Position position;        // 绝对位置
    Meter meter;
    MetricGrouping grouping;
    std::int64_t barIndex;    // 该事件处的绝对小节号
};
```

### 2.4 MeterMap

有序事件序列。第一个事件必须在 `Position 0`。事件之间严格递增。查询：

- `eventAt(position)`：`position` 处生效的拍号事件（事件起点右连续）；
- `meterAt` / `groupingAt` / `barLengthAt`；
- `isInRange`：仅非负位置有效；
- `isBarStart` / `isBeatStart`；
- `barIndexAt` / `barOffsetAt` / `barStart` / `positionOfBar`；
- `toMetric` / `toPosition`；
- `resolveDuration` / `resolveEnd`；
- `metricWeight`；
- `validate` / `describe`。

## 3. 不变式与边界

1. 初始事件固定在 `Position 0`；`MeterMap` 永不为空。
2. 拍号变化必须落在**小节边界**：
   - `position > 上一事件位置`；
   - `position` 是当前 Map 的小节起点；
   - 新事件的 `barIndex` 严格大于上一事件。
3. **拍中变拍号不支持**：拒绝并给出 `meter-change-invalid` / `meter` 诊断，不近似。
4. 负位置不是合法度量坐标：`toMetric`、`positionOfBar`、`toPosition` 对负值抛 `mpl::Error`；`eventAt` 为查询方便夹到初始事件，但调用者必须先检查 `isInRange`。
5. 拍号分母必须为 2 的幂；分子为正。
6. grouping 非法（空、非正、总和不等）在写入时拒绝。
7. `validate()` 重新构建 Map 并检查事件顺序、barIndex 一致性与边界条件。

## 4. 精确换算

设某事件的位置为 `p0`，小节号为 `b0`，其小节长度 `L = n/d`，分母为 `d`，拍长 `1/d`。

正位置 `p >= p0`：

```text
barIndex  = b0 + floor((p - p0) / L)
barOffset = (p - p0) - barIndexDelta * L
beat      = barOffset * d                  // 以 1/d 为单位
```

反向：

```text
position = positionOfBar(bar) + beat * (1/d)
```

`beat` 必须满足 `0 <= beat < numerator`。`positionOfBar(bar)` 从最后一个 `barIndex <= bar` 的事件外推；`bar < 0` 非法。

所有运算使用 `Rational`，不使用浮点中间值。

## 5. `bars` / `beats` 单位的锚点依赖

语言层 `bars`、`beats` 字面量携带“数量 + 单位”，在 Lowering 时由当前 `MetricScope`（MeterMap + anchor）解析为绝对 `Duration`。

### 5.1 Bars

- **正整数 N**：从 anchor 出发，跨过 N 个小节边界，返回精确位置差。
  - anchor 必须是小节起点，否则诊断 `meter-metric-duration`（“bars are anchored at … which is not a bar boundary”）。
  - 支持跨拍号变化，因为逐小节行走。
- **正分数**（如 `1/4 bars`）：按 anchor 处生效的小节长度线性缩放。
  - 不跨拍号变化语义（避免隐式选择）；文档明确这是“锚定的小节分数”。
- **负数**：按 anchor 处小节长度线性缩放（用于 `shift` 的负偏移等），不反向跨拍号行走。

### 5.2 Beats

`amount * beatLength(meterAt(anchor))`。跨拍号变化时仍使用 anchor 处拍长；这是显式锚定语义。

### 5.3 Whole / 裸有理数

绝对全音符时值，不依赖 anchor。

### 5.4 典型结果

7/8、grouping 2+2+3：

| 表达式 | anchor | 结果 |
| --- | --- | --- |
| `2 bars` | 0 | `7/4` |
| `1 bars` | `7/8` | `7/8` |
| `1/4 bars` | 0 或 `7/8` | `7/32` |
| `2 beats` | 0 | `2/8` |

4/4：

| 表达式 | anchor | 结果 |
| --- | --- | --- |
| `2 bars` | 0 | `2` |
| `2 beats` | 0 | `1/2` |
| `2 beats` | `3`（4/4 事件处） | `1/2` |

## 6. Section 与 Clip 的时间解析

### 6.1 Section 局部 MeterMap

```text
section A(4 bars) meter 7/8 grouping 2+2+3 {
  meter 5/8 at 2 bars grouping 2+3;
  clip Guitar ...
}
```

- Section 声明自己的初始拍号与分组；未声明时使用 **project 初始拍号**，而不是继承前一段落。
  这样 Section 内容在局部坐标中 lower 一次，重复播放不会因播放位置不同而产生不同 IR。
- Section 内的 `meter ... at ...` 声明相对 Section 起点的拍号变化；位置必须为小节边界。
- Section 的 `duration` 与 clip `at` 均在该局部 MeterMap 下解析，anchor 为局部 0。
- Section 的 clip `play` 表达式以 clip 的局部 offset 作为度量 anchor；函数内 `bars`/`beats` 因此相对 clip 放置位置解析。
- **Section 长度必须落在小节边界**：`section.duration` 必须是 Section 局部 Map 的小节起点。否则诊断 `section-not-bar-aligned`，因为下一 Section 将从小节中间开始。
- 允许 clip 内容短于 Section（逻辑留白），也允许内容超出（产生 `sema-section-overflow` 警告），但 Section 边界本身必须对齐。

### 6.2 Arrangement 全局 Map

Arrangement 按播放顺序拼接 Section：

1. 第一个 Section 的首事件成为全局 Map 的初始事件；
2. 后续 Section 的首事件在 `cursor` 处；若与当前拍号/分组相同则跳过重复事件，否则插入；
3. Section 内部变化平移到 `cursor + localPosition`；
4. `cursor += section.duration`；
5. 所有 Section 结束后写入 `Project::meterMap()`，再进入 Production IR。

由于每个 Section 长度都是整数小节，`cursor` 始终位于小节边界。

## 7. Metric Hierarchy

`MeterMap::metricWeight(position)` 返回：

```cpp
struct MetricWeight {
    int level;              // 3 bar, 2 group, 1 beat, 0 subdivision
    double weight;          // level / 3.0，仅作稳定数值投影
    bool barStart;
    bool groupStart;
    bool beatStart;
    int groupIndex;
    int beatIndex;
    Rational subdivision;   // 拍内偏移
};
```

7/8 = 2+2+3 的层级：

| 位置 | level | 含义 |
| --- | --- | --- |
| 0 | 3 | 小节 |
| 1/8 | 1 | 拍 |
| 2/8 | 2 | 组起点 |
| 3/8 | 1 | 拍 |
| 4/8 | 2 | 组起点 |
| 1/16 | 0 | 细分 |
| 5/8、6/8 | 1 | 拍 |

7/8 = 3+2+2 时 `3/8` 与 `5/8` 变为组起点，`2/8`、`4/8`、`6/8` 为拍。

`level` 是可查询的结构权重，不是审美评价。实际演奏力度由 `metricAccent` 或显式力度决定。

## 8. MIDI 映射

- 每个 `ProductionIR::meterMap` 事件输出一个标准 MIDI `FF 58` 时间签名元事件；
- tick 由绝对位置与 PPQ 精确换算；无法整除时抛 `midi` 错误，不静默舍入；
- MIDI 没有标准的节拍分组元事件；grouping 写入 Production IR 与 Trace，`MidiBackend::describe()` 以 `meter 7/8 (2+2+3)` 显示；
- Tempo Map 与 Meter Map 独立；本阶段不实现变速。

示例（PPQ=480，132 BPM 不影响 tick）：

```text
0      7/8 (2+2+3)
7/2    5/8 (2+3)      tick 6720
6      4/4 (2+2)      tick 11520
```

## 9. 诊断

| 代码 | 条件 |
| --- | --- |
| `invalid-meter` | 分子非正或分母非 2 的幂 |
| `invalid-meter-grouping` | grouping 非法或总和不等 |
| `invalid-meter-map` | 事件逆序、非小节边界、barIndex 不一致 |
| `meter-change-invalid` | Section 内拍号变化位置非法/不在小节边界 |
| `meter-metric-duration` | `bars` 锚点不是小节起点，或解析失败 |
| `section-not-bar-aligned` | Section 长度不落小节边界 |
| `negative-position` | Production 事件位置为负 |
| `midi` | 拍号事件无法在 PPQ 下精确表示 |

## 10. 范围外

- 拍中变拍号（小节内拍号变化）；
- 完整 Tempo Map / 变速系统；
- 非 2 的幂分母、复合拍号（如 3+2/8 的“分子相加”记法，用 grouping 表达）；
- MIDI 之外的 DAW 拍号分组元数据；
- 以拍号驱动的音频速率求值。
