# MPL V0.3.0-beta 开发报告：节奏代数与度量语义

日期：2026-11-25

## 1. 结论

V0.3.0-beta 在保留 V0.3.0-alpha 全部 158 项测试的基础上，建立了：

- 作品内部的 Meter Map、变拍号与精确小节坐标；
- 非均匀节拍分组（7/8 = 2+2+3、3+2+2）与 Metric Hierarchy；
- 独立、类型化、可序列化的 Rhythm 值；
- 有界节奏代数（sequence/overlay/repeat/rotate/shift/stretch/subdivide/accent/realign/quantize）；
- Rhythm × Pitch 的显式组合（zip/cycle）；
- 基础跨声部起音关系（intersect/union/difference/complement/accentFollow）；
- 函数度量上下文缓存键与独立实例验证；
- 升级后的 MRP-001（7/8 → 5/8 → 4/4）及 MIDI/WAV/IR/Trace；
- MRP-001 Semantic Benchmark。

全量测试 **237/237** 通过（Debug 与 Release），新增 79 项；既有 158 项全部保留。

## 2. 架构审查

见 `docs/reports/v0.3.0-beta-architecture-review.md`。

结论：不重写 Temporal IR，不引入 VM/LLVM。实际缺陷集中在：

1. 拍号只有全局 `Meter`，`ProductionIR::meterMap` 只能写一个事件；
2. `bars`/`beats` 在 Sema 折叠，无法表达起点依赖；
3. 没有独立 Rhythm；
4. 函数缓存键只有单一 Meter，没有调用锚点。

修改集中在 MeterMap/Rhythm 两个新值系统、前端度量上下文与 Production/MIDI 数据打通。

## 3. Meter Map 与变拍号

### 3.1 数据模型

- `MeterMap`：有序事件、初始事件在 0、每个事件携带 `barIndex` 与 `MetricGrouping`；
- 拍号变化必须落在小节边界；拍中变化拒绝；
- 精确 `Position <-> MetricPosition` 双向转换；
- `MetricDuration`（N 小节/N 拍）需要锚点才能折叠；
- `MetricWeight`：level 3/2/1/0 = 小节/组/拍/细分。

### 3.2 语言

```text
section A(4 bars) meter 7/8 grouping 2+2+3 {
  meter 5/8 at 2 bars grouping 2+3;
  clip Guitar { play RiffA; }
}
```

- Section 未声明拍号时使用 project 初始拍号，不继承前一段落；
- Section 内容在局部坐标中 lower 一次，可安全重复播放；
- Section 长度必须落在小节边界；
- Arrangement 按播放顺序拼接全局 Meter Map。

### 3.3 bars/beats 的起点依赖

- 正整数 `bars` 逐小节行走；anchor 必须是小节起点；
- 分数/负数 `bars` 按 anchor 处小节长度线性缩放；
- `beats` 使用 anchor 处拍长；
- 全局 let anchor=0；Section 内容 anchor=局部 0；Clip `play` anchor=局部 offset；
- 失败为 `meter-metric-duration`，不静默近似。

### 3.4 Production / MIDI / Trace

- `ProductionIR::meterMap` 携带 grouping 与 barIndex；
- MIDI 为每个事件输出 `FF 58` 时间签名；
- Trace 增加 `meterMap` 与函数 `metricContext`；
- Tempo Map 保持独立。

MRP-001 实际结果：

```text
0      7/8 (2+2+3)   bar 0    tick 0
7/2    5/8 (2+3)     bar 4    tick 6720
6      4/4 (2+2)     bar 8    tick 11520
logical extent = 10 whole notes
```

## 4. Rhythm 类型

`Rhythm` 包含：

- 逻辑长度；
- 事件 onset/duration；
- accent（0..8）；
- 显式 rest；
- 每事件 origin；
- 与 Fragment 共用 Music IR ObjectId 空间，但求值类型分离。

UI/语言：

```text
let R3 = rhythm {
  hit 1/8 accent 2,
  hit 1/8,
  hit 1/16,
  rest 1/16
};
```

IR 节点：`rhythm-literal`、`rhythm-sequence`、`rhythm-overlay`、`rhythm-repeat`、
`rhythm-rotate`、`rhythm-shift`、`rhythm-stretch`、`rhythm-subdivide`、
`rhythm-accent`、`rhythm-realign`、`rhythm-quantize`、`rhythm-accent-by-meter`、
`rhythm-intersect`、`rhythm-union`、`rhythm-difference`、`rhythm-complement`、
`rhythm-accent-follow`、`rhythm-combine`。

设计决定见 ADR-0021。

## 5. 节奏代数

| 算子 | 语义要点 |
| --- | --- |
| sequence | 顺序拼接，结果长度求和 |
| overlay | 0 对齐叠加，保留重叠 |
| repeat | 1..256 次 |
| rotate | 周期内相位；跨边界事件精确切分 |
| shift | 线性平移，可为负，不环绕 |
| stretch | 正有理数缩放 |
| subdivide | 每事件切 n 份，accent 复制 |
| accent | 起音按 onset 顺序循环 levels |
| realign | 扩展到周期的整数倍 |
| quantize | 最近 grid，平局向上 |

共同周期：

```text
lcm(3/8, 7/8) = 21/8
lcm(3/8, 1/2) = 3/2
```

容量：事件 4096、repeat 256、subdivide 64、有理数分子/分母 1e9。超出报诊断，不无界展开。设计决定见 ADR-0023。

## 6. 节拍分组与层级

- `MetricGrouping` 以分母单位表达，例如 7/8 = {2,2,3}；
- 7/8 的 2+2+3：level 3 在小节、2 在 2/8 与 4/8、1 在拍、0 在细分；
- 7/8 的 3+2+2：2 在 3/8 与 5/8；
- `metricAccent(rhythm)` 把层级写入 accent；
- `metricWeight(offset)` 提供语言查询；
- `syncopate` 使用显式 MeterMap + anchor 判断拍点；
- 结构重音与实际力度分离：只有 Rhythm × Pitch 才按 `base + accent*8` 映射力度。

设计决定见 ADR-0022。

## 7. Rhythm × Pitch

- `zip` 严格相等，`cycle` 循环填充；
- 休止不消耗音高；
- 负 onset 不能进入 Fragment 时间轴；
- Fragment span = Rhythm 逻辑长度；
- 力度 = `clamp(base + accent*8, 1, 127)`；
- note 记录 `rhythmOrigin` 与 `rhythmEventIndex`，逐音可追溯。

## 8. 跨声部关系

- `onsetSet(r, unit)` 把节奏归约为纯起音集合；
- `intersect`/`union`/`difference` 可选 grid；
- `complement(r, span, grid)`；
- `accentFollow(source, target, [grid])`；
- 关系只处理起音集合，不处理完整持续区间；
- 结果逻辑长度 = 共同周期；
- 不引入通用约束求解器。

设计决定见 ADR-0024。

## 9. 函数身份与缓存

- Music Value / Function Invocation / Performance Instance 继续分离；
- 缓存键：`fn:name:v<version>:k<key>:mc<mapId>@<anchor>|args`；
- 同参同定义同上下文共享 Music IR 对象；
- 不同 Section 的度量上下文不同，不错误共享；
- 每个 Clip 独立 `InstanceId` 与 `PerformanceExpression`；
- 调用链上的定义变化改变结果；
- 无关 Section 的拍号变化不改变另一 Section 的结果；
- Trace 的 `functionCalls` 增加 `metricContext`。

设计决定见 ADR-0025。

## 10. MRP-001 升级

源：`examples/math-rock.mpl`

- 段落：A 4 bars 7/8 (2+2+3)；B 4 bars 5/8 (2+3)；Reprise 4 bars 4/4 (2+2)；
- Rhythm：R3（3/8 主题细胞）、R7、R5、R4；
- 周期：`commonPeriod(R3, R7) = 21/8`；`realign` 后 union；
- 两个不同长度周期：3/8 与 7/8；
- 跨声部：B 段鼓 `accentFollow(Displaced5, R5, 1/8)`；
- 主题再现：A 段 `VerseTheme`（Variant 变体），Reprise 原形 `Theme`；
- 用户函数：cell/counter/lead/drums7/drums5/drums4；
- 局部参数：`Variant`、`Seed`；
- 鼓的重音来自 `metricAccent`，即 Section 的 2+2+3 / 2+3 / 2+2 分组。

### 产物

```text
mplc build examples/math-rock.mpl -o build/math-rock --tail 1.0
```

| 属性 | 值 |
| --- | --- |
| Music IR | 83 objects |
| Production | 4 tracks, 12 clips, 226 notes |
| 逻辑长度 | 10 whole notes |
| 时长 | 18.18 s（不含尾音） |
| WAV | 48 kHz stereo float32, 921,818 frames |
| Sample Peak | -4.01 dBFS |
| True Peak | -3.90 dBTP |
| Integrated | -15.92 LUFS |
| Short-term max | -15.00 LUFS |
| Function calls | 8 |

文件：

- `build/math-rock/math-rock.mid`
- `build/math-rock/math-rock.wav`
- `build/math-rock/math-rock.music.ir`
- `build/math-rock/math-rock.production.ir`
- `build/math-rock/math-rock.temporal.ir`
- `build/math-rock/math-rock.trace.json`

## 11. 语义基准

见 `docs/reports/mrp-001-semantic-benchmark.md`。

15 项 `Benchmark*` 测试覆盖：变拍号、分组层级、独立 Rhythm、双周期、精确 realign、
复合起音、Rhythm × Pitch 计数、主题再现、用户函数、MIDI tick、WAV timeline、
Temporal IR 绝对时间映射、随机算法版本契约、拍中变拍号诊断、严格 zip 诊断。

艺术评价不进入自动验收。

## 12. 测试

| 测试文件 | 数量 |
| --- | --- |
| test_core | 25 |
| test_audio | 13 |
| test_signal | 20 |
| test_analysis | 15 |
| test_temporal | 16 |
| test_functions | 15 |
| test_frontend | 15 |
| test_meter | 22 |
| test_rhythm | 26 |
| test_relations | 13 |
| test_benchmark | 15 |
| test_backend | 9 |
| test_lowering | 8 |
| test_ir | 6 |
| test_golden | 6 |
| test_semantics | 13 |
| **合计** | **237** |

Debug 与 Release 均 237/237。既有 158 项全部保留；新增 79 项。

## 13. 文档与 ADR

- 架构审查：`docs/reports/v0.3.0-beta-architecture-review.md`
- Meter Map 规范：`docs/meter-map-spec.md`
- 节奏语义规范：`docs/rhythm-semantics.md`
- 随机流契约：`docs/random-stream-contract.md`
- 更新 EBNF：`docs/grammar.ebnf`
- 语言规范：`docs/language-spec.md` §15
- 算子规范：`docs/operator-spec.md` §10
- ADR-0020 Meter Map 与锚定度量时值
- ADR-0021 Rhythm 作为 Music IR 值节点
- ADR-0022 节拍分组与 Metric Hierarchy
- ADR-0023 有界节奏代数与有理周期
- ADR-0024 跨声部起音关系
- ADR-0025 函数身份与度量上下文缓存键
- Semantic Benchmark：`docs/reports/mrp-001-semantic-benchmark.md`
- 技术债务：`docs/technical-debt.md`

## 14. 范围限制

本阶段未实现：

- LLM 集成；
- 情绪映射、Taste 学习、审美评分；
- 通用约束求解；
- 完整泛型、任意递归、函数值；
- 完整插件宿主、SoundBoxer Adapter；
- 大规模 DSP 扩展；
- 拍中变拍号；
- 完整变速系统 / 多 Tempo 事件；
- 模块/导入；
- 跨编译持久 Lowering 缓存；
- 区间级跨声部关系与声部进行检查。

## 15. 下一步

1. 模块系统与标准库（std/rhythm、std/melody）；
2. 完整 Tempo Map 与变速；
3. 拍中变拍号或复合拍号记法的扩展；
4. 跨编译增量 Lowering 与持久缓存；
5. 区间级跨声部关系与声部进行诊断；
6. 受控递归/循环的资源与终止语义。

本阶段成功标准已满足：MPL 能以精确有理时间表达、计算并验证数学摇滚中具有代表性的复杂时间与节奏结构；不支持的语义有明确诊断，而不是隐藏近似。
