# ADR-0022：节拍分组与 Metric Hierarchy

状态：已采纳（V0.3.0-beta）

## 背景

7/8 可以被感知为 2+2+3、3+2+2 或其他分组。Alpha 只有 `Meter` 与手工力度，无法查询结构层级，也没有把“结构重音”与“实际演奏力度”分开。

## 决策

1. `MetricGrouping`：
   - 以拍号分母为单位的分组向量，例如 7/8 = {2,2,3}；
   - 总和必须等于分子；空向量规范化为每拍一组；
   - grouping 不改变小节长度。
2. `MeterMap` 的每个事件携带 grouping；Section 头与 Section 内变拍号都可声明。
3. `MeterMap::metricWeight(position)` 提供最小查询接口：
   - level 3 = 小节起点；
   - level 2 = 组起点；
   - level 1 = 拍起点；
   - level 0 = 拍内细分；
   - 同时返回 barStart/groupStart/beatStart、groupIndex、beatIndex、subdivision。
4. 语言层 `metricWeight(offset)` 返回层级整数；`metricAccent(rhythm)` 把层级写为 Rhythm 的 accent 属性。
5. 结构重音（accent）与实际力度分离：
   - Rhythm accent 是抽象属性；
   - Rhythm × Pitch 才按显式公式 `clamp(base + accent*8, 1, 127)` 映射为 MIDI velocity；
   - 不把层级直接当作演奏力度。
6. `syncopate` 使用显式 MeterMap + anchor 判断拍点，而不是单一全局拍号。

## 后果

- 7/8 的 2+2+3 与 3+2+2 可被精确查询与测试。
- 结构层级可驱动鼓/重音，也可完全忽略。
- 不引入“节奏复杂度评分”或审美判断；层级只是结构查询。
- 拍中变拍号与更细的层级（subdivision 权重）留给后续版本。
