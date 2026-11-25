# MPL 音频分析与生产反馈理论规范（V0.2.0）

## 1. 音频分析与音乐计算的关系

音乐计算回答“音乐结构是什么”：音高、时值、段落、轨道、演奏表情与混音连接。音频分析回答“渲染结果在物理上是什么”：峰值、能量、动态与响度。

两者是不同层次：

- Music/Temporal/Production IR 是符号与结构层，不包含 LUFS、dBTP、RMS。
- AudioBuffer 是物理层，只包含采样值。
- Audio Analysis 是物理层的测量，不回写符号结构。
- Production Feedback 读取测量结果，修改生产参数，再重新渲染；它不修改音乐内容本身。

因此，分析结果不能被当作 Music IR 的属性，也不能让 RMS 或 LUFS 参与音符级语义。

## 2. 技术生产目标与主观感知目标

技术目标可测量、可重复、可用数值验证：

- Integrated Loudness 目标（LUFS）；
- True Peak / Sample Peak 约束（dBTP/dBFS）；
- RMS、Crest Factor、Short-term Loudness 等诊断指标；
- 处理器参数范围与状态语义。

主观感知目标不可由当前系统判定：

- 好听、温暖、通透、有冲击力；
- 情绪、风格、审美、Taste；
- “更响就是更好”。

V0.2.0 只处理技术目标。更响的音频不被视为更好的音频；响度目标只是交付约束，不是质量评价。

## 3. 直接执行与目标驱动执行

直接执行（direct execution）：

```text
参数 → SignalGraph → AudioBuffer
```

用户明确给出每个参数，系统只负责正确执行。V0.2.0-beta 及以前属于这一层。

目标驱动执行（target-driven execution）：

```text
程序 + 资产 + 运行配置 + 生产目标
  → Render → Analyze → Adjust → Render → Verify
```

用户给出目标与约束，系统在有限预算内求解一组生产参数。V0.2.0 引入最小目标驱动执行：只求解 master gain 与 limiter ceiling，不做通用约束求解。

## 4. 离线生产反馈

Production Feedback 是跨渲染阶段的闭环：

- 它运行在 SignalGraph 之外，不是图节点，也不是图环路。
- 每轮完整渲染、完整分析，然后做确定性参数调整。
- 反馈只修改 Production IR 的生产参数（master gain、limiter ceiling），不修改 Music/Temporal IR。
- 每轮结果必须可复现：相同输入、目标、迭代预算得到相同输出。
- 反馈需要同时记录“参数修改”和“测量结果”，否则不可审计。

音频信号反馈（如 Compressor sidechain、Delay feedback）属于实时信号路径，遵循图规则；Production Feedback 属于编译/生产阶段，两者不得混同。

## 5. 有限迭代与失败语义

目标驱动执行必须承认失败：

- 目标可能在给定峰值约束下不可满足（例如极高 LUFS + 极低 dBTP）。
- 迭代必须有上限，禁止无限搜索。
- 达到上限但未满足目标，必须产生明确诊断，而不是静默返回。
- 每轮参数修改必须有最小步长；步长退化为 0 时视为不可满足。
- 失败时仍返回最后一轮的有效渲染与分析，供用户检查。

V0.2.0 的定义：

- 容差：`loudnessToleranceLu`；
- 预算：`maxIterations`；
- 不可满足：`mastering-unsatisfiable`；
- 峰值超限：`mastering-peak-exceeded`；
- 预算耗尽：`mastering-max-iterations`；
- 静音输入：`mastering-silent`。

## 6. 音乐程序、资产、运行环境与生产目标

一次生产由四类输入共同决定：

| 输入 | 作用 | 是否属于音乐内容 |
| --- | --- | --- |
| 音乐程序（Music/Temporal IR） | 音高、时间、表情、结构 | 是 |
| 资产（AssetBinding/Resolver） | 音色、采样、音源版本 | 是，但可替换 |
| 运行环境（采样率、Block、声道、终止策略） | 物理渲染条件 | 否 |
| 生产目标（LUFS、dBTP、容差、迭代） | 交付约束 | 否 |

同一音乐程序 + 不同资产 = 不同音色；同一程序 + 不同运行环境 = 不同采样精度；同一程序 + 不同生产目标 = 不同母带结果。四者必须分别记录，才能复现一次生产。

## 7. 感知策略层边界

情绪、审美、Taste、风格偏好属于独立的感知策略层：

- 不进入 Music IR、Temporal IR、Production IR。
- 不进入 Audio Analysis 的技术指标定义。
- 可以消费技术指标与分析结果，但不能把主观判断伪装成技术测量。
- V0.2.0 不实现该层，也不为它预留隐式语法。

## 8. 与 SignalGraph 的关系

- SignalGraph 负责采样级正确执行与通道/状态语义。
- Audio Analysis 只读渲染结果。
- Production Feedback 在完整渲染之间调整有限个生产参数。
- 三者共享 AudioBuffer 与 Diagnostic，但不共享控制流：图内没有分析节点，分析不触发图重编译，反馈不修改图拓扑。
