# MRP-001 工程需求与语言能力差距分析

项目：Math Rock Reference Project（MRP-001）

日期：2026-11-25

## 1. 定位

MPL 是具有明确执行语义的音乐生产程序设计语言，不定义普适情绪映射或审美标准。MRP-001 用真实数学摇滚工程需求驱动语言抽象，而不是反过来为已有实现寻找用例。

情绪、风格、Taste 与自然语言意图由外部系统或人承担；MPL 负责执行已经确定的音乐决策，并保证语义、复现与溯源。

## 2. MRP-001 工程需求

### 2.1 音乐结构

- 全局 7/8 拍；
- 3/8 动机单元在 7/8 小节内循环与相位移动，形成 3-against-7 的复合周期感；
- 4 小节段落 A、B、Reprise；
- 主题再现：A 段动机在 Reprise 原样/变体返回；
- 吉他、贝斯、鼓、Lead 四个声部；
- 参数化变体：修改 `Variant` / `Seed` 产生不同版本。

### 2.2 语义要求

- 精确有理时值，不允许浮点时间漂移；
- 动机、节奏、演奏力度可组合；
- 周期与相位计算精确；
- 相同输入可复现；
- MIDI 与 WAV 忠实执行可支持语义；
- 不支持的语义必须诊断，不能静默近似；
- 生成结果可追溯到函数调用与内部算子。

## 3. 现有能力审查

### 3.1 Rational 与时间

已具备：

- 全音符有理数；
- 精确比较、加减乘除；
- 小节/拍单位在 Sema 中换算；
- 7/8 的 `barLength = 7/8`。

缺口：

- 小节/拍换算依赖全局 Meter，不能按段落切换；
- 没有“节拍分组”（如 7/8 = 2+2+3）的类型或属性；
- 没有 Metric Hierarchy（强/弱拍层级）表示。

### 3.2 Music IR

已具备：

- Fragment 含显式 span；
- Note 含 offset/duration/velocity/origin；
- 确定性算子：transpose/reverse/stretch/repeat/sequence/parallel/diatonicTranspose/harmonize/syncopate；
- V0.3.0-alpha 新增 `rotate`（span 内循环相位移动）。

缺口：

- 没有独立 Rhythm 对象；节奏只能作为 Fragment 的 offset/duration 集合；
- 没有周期/相位元数据，`rotate` 的周期来自 Fragment span；
- 没有跨声部节奏关系检查；
- 没有节拍分组或 accent pattern 的显式表示；
- 没有和弦对象与声部连接检查。

### 3.3 Temporal IR

已具备：

- 曲线图、精确时间域、自动化绑定；
- 相对域演奏表情。

缺口：

- 没有音频速率求值；
- 曲线构造点仍为常量，函数参数只能选择曲线或数值参数；
- 没有节拍层级驱动的曲线模板。

### 3.4 Typed HIR 与函数

已具备（alpha）：

- 用户函数、参数/返回类型、局部 let、嵌套调用；
- 显式 `context key, meter`；
- Bool/if/for/有限集合；
- 函数缓存与调用 Trace。

缺口：

- 不支持递归、提前 return、循环控制流、模块/导入；
- 函数缓存只在单次编译内；
- 没有函数值/高阶函数；
- 没有错误处理。

### 3.5 Production IR

已具备：

- Track/Clip/InstanceId/PerformanceExpression；
- `meterMap` 容器存在，但 Compiler 只写入初始 Meter 事件；
- logicalExtent 与 MixSpec。

缺口：

- 语言层没有段落变拍号声明；
- `bars`/`beats` 单位在 Sema 阶段按全局 Meter 换算，变拍号需要按段落上下文重算；
- MIDI Backend 尚未接收多 Meter 事件；
- 没有跨声部对齐/冲突诊断。

## 4. 差距与 alpha 决策

| 语义 | 现状 | alpha 决策 |
| --- | --- | --- |
| 7/8 全局拍号 | 支持 | 已用于 MRP-001 |
| 变拍号 / Meter Map | 数据容器存在，语言/Compiler/MIDI 未打通 | 推迟到 beta |
| 节拍分组 2+2+3 | 无表示 | 推迟；用 3/8 单元与 rotate 近似复合周期 |
| Metric Hierarchy / Accent | 仅 velocity | 推迟；用力度值手工表达 |
| 独立 Rhythm 对象 | 无 | 推迟；Fragment 继续承载节奏 |
| 周期/相位 | `repeat` + 新 `rotate` | 已支持精确有理相位 |
| 多周期/复节奏 | `parallel`/`sequence` + 不同 span | 部分支持；无自动对齐检查 |
| 跨声部节奏关系 | 无 | 推迟；靠显式编写与测试 |
| 动机变奏/再现 | 函数 + 变换 | 已支持 |
| 和弦排列/声部连接 | harmonize/parallel | 部分支持；无连接检查 |
| 结构约束验证 | 类型/范围/片段 span | 部分；无节拍分组约束 |

## 5. MRP-001 的 alpha 实现

`examples/math-rock.mpl`：

- 全局 7/8；
- `cell`：1/8 + 1/8 + 1/16 + 1/16 的 3/8 单元；
- `riff`：`for` 循环 8 个 cell，每 3 个使用 `rotate(ratio(i,16))` 做相位移动；
- `lead`：`random(seed,i)` 选择音符/休止，`round` 生成力度；
- `drumBar` / `bassBar`：7 个 1/8 栅格；
- A/B/Reprise 结构，`RiffA` 在 A 与 Reprise 复用；
- `Variant`/`Seed` 作为全局参数；
- 函数调用与内部算子进入 Trace。

## 6. 明确未满足的 MRP 语义

1. 不能声明 A 段 7/8、B 段 5/4 的变拍号；
2. 不能表达 7/8 = 2+2+3 的分组，也不能让算法读取分组；
3. 没有独立 Rhythm 对象，节奏不能脱离音高单独参数化；
4. 没有跨声部节奏对齐/冲突诊断；
5. 没有和弦声部连接规则；
6. 函数缓存不跨编译；
7. 无递归/循环控制流/模块。

这些不是实现缺陷，而是 alpha 范围外的语言能力缺口，已列入技术债务与 beta 设计。

## 7. 修订后的 V0.3.0-alpha 范围

实现：

- 用户函数、显式类型、局部作用域、嵌套调用；
- 显式 Key/Meter 上下文与缓存键；
- Typed HIR 求值，保留 Music IR 依赖；
- Bool/if/for/有限集合；
- 显式种子随机原语；
- `note`/`rest`/`rotate`/`ratio` 等音乐原语；
- 调用 Trace 与函数缓存；
- MRP-001 Demo（全局 7/8 + 3/8 相位循环）。

不实现：

- Meter Map / 变拍号；
- 独立 Rhythm 对象；
- 节拍分组与层级；
- 跨声部节奏分析；
- 递归、模块、泛型、函数值。

## 8. 验收口径

Semantic Fidelity 优先：

- 7/8 与 3/8 周期的 note offset 精确；
- 相同参数/种子产生相同 MIDI/WAV；
- 修改 `Variant`/`Seed` 产生预期变化；
- 函数程序与等价内联程序 MIDI 一致；
- 独立函数修改不影响无关输出；
- 不支持语义有明确诊断；
- 审美评价不进入自动验收。

## 9. V0.3.0-beta 状态（回填）

本报告记录的是 alpha 时点的差距。V0.3.0-beta 已关闭：

1. 变拍号 / Meter Map（Section 局部 Map + 全局拼接）；
2. 7/8 的 2+2+3 / 3+2+2 分组与 Metric Hierarchy；
3. 独立 Rhythm 对象与节奏代数；
4. 基础跨声部起音关系（交/并/差/补/accent follow）；
5. MIDI 多拍号事件与 WAV 时间映射；
6. 函数度量上下文缓存键与独立实例验证。

仍未关闭：

- 和弦对象与声部连接检查；
- 通用跨声部约束求解；
- 跨编译持久缓存；
- 模块/导入、递归、函数值。

新实现与验证见 `docs/reports/mpl-v0.3.0-beta.md` 与
`docs/reports/mrp-001-semantic-benchmark.md`。

## 10. V0.3.0-rc 状态（回填）

V0.3.0-rc 已关闭：

- 和弦对象、扩展和弦、转位、开放排列；
- 共享和声时间线与查询；
- 有界声部连接搜索与状态分类；
- 音乐关系 Analyze/Validate/Construct；
- 最小模块与标准库；
- 乐器能力边界（吉他示例）。

仍未关闭：

- 通用约束求解、风格化声部进行；
- 完整吉他指法求解器；
- 跨编译持久缓存；
- 模块命名空间/包管理；
- Tie / 跨周期连音线。

新实现见 `docs/reports/mpl-v0.3.0-rc.md`。
