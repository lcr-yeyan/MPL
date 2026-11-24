# ADR-0012：音频时间、采样舍入、自动化与三类时长

状态：已采纳（V0.2.0-alpha）

## 背景

音频执行必须明确 MusicalTime → PhysicalTime → SampleTime 的转换、Block 内自动化行为，以及 Section、内容与尾音的边界，避免把符号时间与渲染时间混为一谈。

## 决策

1. Tempo map 为分段常速；`AudioClock` 提供双向换算。
2. 事件采样位置使用 `llround(seconds × sampleRate)`；终点至少比起点晚 1 sample。
3. 实时控制按 Block 边界求值并在 Block 内线性插值；音符事件保持采样精确。
4. 三种时长分离：
   - Section Logical Extent：排程，不截断；
   - Musical Content Extent：内容结束；
   - Rendered Audio Extent：`max(note end + release) + tailSeconds`。
5. Section 越界在语言层为警告，允许重叠。
6. 线性自动化必须 Block Size 无关；非线性曲线允许 Block 边界近似，未来可逐样本求值。

## 后果

- 事件位置与渲染 Block 解耦，WAV 可复现。
- 尾音与段落边界语义明确，不会误截断 release。
- 非线性自动化精度是已记录限制。
