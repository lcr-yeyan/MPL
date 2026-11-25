# MPL 技术债务清单

更新：2026-11-25（V0.3.0-rc）

本清单记录已确认、但不阻塞当前阶段交付的问题。不是愿望列表；每项都有触发条件与影响范围。

## 1. 音频与母带

| 编号 | 问题 | 影响 | 计划 |
| --- | --- | --- | --- |
| TD-01 | Step/非线性自动化在 Block 边界采样 | 64/512 Block 存在容差级差异 | beta 引入更细粒度或逐样本求值 |
| TD-02 | 无通用 PDC | 并行路径处理延迟不一致时只诊断，不自动对齐 | 未来 Adapter/图扩展 |
| TD-03 | Limiter 是 Sample Peak Limiter | 不保证 dBTP；强限制下 True Peak 可高出约 3 dB | 后续实现 4x 处理 True Peak Limiter |
| TD-04 | True Peak 为 49-tap 4x 过采样估计 | 不是 ITU 系数表逐位复刻，不用于合规认证 | 可替换为 ITU/参考实现 |
| TD-05 | True Peak/响度分析为 O(N × taps) | 长音频分析开销较高 | 后续优化/分块 |
| TD-06 | Delay/Reverb 尾音上限固定 10 s | 极长尾音被截断 | 可配置或按处理器声明 |
| TD-07 | 无 pre-fader Send 选项 | 只能 post-fader | 后续混音语法扩展 |
| TD-08 | Master insert 在 Master Fader 之前 | 母带反馈需按 fader 增益补偿 limiter ceiling | 已记录；未来可提供 post-fader 插入槽 |

## 2. 生产反馈

| 编号 | 问题 | 影响 | 计划 |
| --- | --- | --- | --- |
| TD-09 | 只求解 master gain 与 limiter ceiling | 无法满足复杂多参数目标 | beta 可扩展有限参数集 |
| TD-10 | 峰值容差固定默认 0.1 dB | 严格性依目标而定 | 已独立为 `peakToleranceDb`，可继续细化 |
| TD-11 | 无全局优化器 | 结果依赖贪心迭代 | 不引入通用约束求解器 |

## 3. 语言与前端

| 编号 | 问题 | 影响 | 计划 |
| --- | --- | --- | --- |
| TD-12 | 函数缓存仅单次编译内 | 跨编译增量 Lowering 未实现 | 后续持久缓存 |
| TD-13 | ~~无模块/导入~~ | 已在 V0.3.0-rc 解决：文件级模块 + 标准库 | 剩余见 TD-40 |
| TD-14 | 不支持递归/提前 return/循环控制流 | 算法表达受限 | 需先定义资源与终止语义 |
| TD-15 | 函数值/高阶函数未实现 | `map` 等需内建 | 后续 |
| TD-16 | 全局 let 可持有标量，但无显式类型标注 | 参数类型靠调用点约束 | 可增加 `let x: Int` |
| TD-17 | 函数名与内建名冲突未专门诊断 | 函数优先，可能遮蔽内建 | 增加保留名检查 |
| TD-18 | 函数版本哈希覆盖 AST 结构，不含源码注释/格式 | 仅语义变化影响版本 | 可接受 |
| TD-19 | 调用 Trace 为扁平记录 + depth | 大型调用树阅读成本高 | 可输出树形结构 |
| TD-20 | 随机只有 `random(seed,index)` | 无跨编辑稳定流 | 已在 ADR-0025 定义算法版本契约；实现留待后续 |
| TD-38 | 无局部可派生随机流 | 多声部/多段落共享 seed 时需手工偏移 | 后续 `randomStream` 与算法版本记录 |

## 4. 节拍与数学摇滚语义

| 编号 | 问题 | 影响 | 计划 |
| --- | --- | --- | --- |
| TD-21 | ~~无 Meter Map / 变拍号语言~~ | 已在 V0.3.0-beta 解决：Section 局部 MeterMap + 全局拼接 | 已解决 |
| TD-22 | ~~无独立 Rhythm 对象~~ | 已在 V0.3.0-beta 解决：Rhythm 值 + Music IR 节点族 | 已解决 |
| TD-23 | ~~无节拍分组 2+2+3 / Metric Hierarchy~~ | 已在 V0.3.0-beta 解决：MetricGrouping + metricWeight/metricAccent | 已解决 |
| TD-24 | ~~无跨声部节奏对齐/冲突诊断~~ | 部分解决：起音集合关系；区间级关系与声部进行仍未实现 | 后续补充区间语义 |
| TD-25 | ~~无和弦对象与声部连接检查~~ | 已在 V0.3.0-rc 部分解决：Chord/Voicing/HarmonyTimeline/voiceLead | 剩余：区间级和声分析与风格规则 |
| TD-26 | MIDI 导出对 humanize timing 量化 | 音频保留亚 tick，MIDI 量化 | 已有警告；可提供微时序导出 |
| TD-31 | 拍中变拍号不支持 | Section 内拍号变化必须在 bar 边界 | 需要“小节内拍号”维度后再开放 |
| TD-32 | 无完整 Tempo Map / 变速 | 只有初始 tempo 事件 | 后续版本 |
| TD-33 | MIDI 无标准 grouping 元事件 | grouping 只在 Production/Trace/describe 中 | 可探索 DAW 专用元事件 |
| TD-34 | 跨声部关系只处理起音 | 长音覆盖/区间重叠不参与节奏关系（音高分析已有 noteOverlap） | 后续区间关系 |
| TD-35 | 函数缓存用 section 序号而非 map 内容哈希 | 相同拍号配置的 Section 不共享缓存 | 可改为 map 内容哈希 |
| TD-36 | 节奏量化只支持直线网格 | 无 swing/shuffle/humanize | 留待风格/律动层 |
| TD-37 | Rhythm 事件数上限 4096 为固定值 | 极长作品可能触发 | 可配置或分块 |
| TD-39 | Tie 未实现；continuation 只检查“存在前驱” | 跨周期持续音在边界截断；同一 onset 多音高的 split 无法用 tie-group 校验 | 后续 Note tie/群组 |
| TD-40 | 模块仅文件级、无前缀调用/别名/版本约束 | 导出名冲突需人工重命名 | 后续命名空间/包管理 |
| TD-41 | 声部连接只支持总移动距离目标 | 无共同音保持、级进优先等风格目标 | 可由 stdlib 显式实现 |
| TD-42 | 乐器只有弦/品位置查询 | 无完整指法/手型求解 | 后续或外部工具 |
| TD-43 | `backend-capability` 状态未由后端实际触发 | 当前只在 API 预留 | 音频/指法后端接入时使用 |
| TD-44 | 任务 `allowedScope` 仅 program/none | 无声明级/区域级 scope | 后续 task DSL |
| TD-45 | `voice_movement` 观察不可从工程自动推导 | V0 返回 unverifiable | 需要显式 Voicing 序列或 IR 标注 |
| TD-46 | Harness 无模型适配器/自动修复循环 | 只能导入外部候选 | 后续供应商适配器 |
| TD-47 | A 组（直接 MIDI JSON）未接入任务验证 | Harness 标记 unsupported | 后续 MIDI JSON 观察适配器 |
| TD-48 | 音频观察需要外部 WAV | 未渲染时 unverifiable | 后续集成渲染步骤 |
| TD-49 | Harness 不自动渲染 | manifest 需受控外部步骤生成 | 后续集成 render/master |
| TD-50 | 参数化 bench 为固定 seed 冻结实例 | 无运行时随机实例生成 | 后续模板生成器 |
| TD-51 | Flat 能力表为静态观察列表 | 新观察需手工更新 | 可改为能力协商 |
| TD-52 | Semantic Delta 不比较 ObjectId/源文本细节 | identity 变化单独标记，不影响 value | 按设计 |
| TD-53 | Frame 的 `unobserved` 仅 objectIds | 新属性需纳入 delta 才受 frame 约束 | 随观察/IR 扩展 |

## 5. 工程

| 编号 | 问题 | 影响 | 计划 |
| --- | --- | --- | --- |
| TD-27 | 全量 Debug 测试约 2 分钟 | 反馈慢 | 可选测试分片 |
| TD-28 | 黄金音频依赖固定算法实现 | 改 DSP 需重生成 | 已按容差/统计设计 |
| TD-29 | 无性能基准 | 长作品性能未知 | 后续加入 |
| TD-30 | 无内置并发/并行编译 | 单线程 | 非当前目标 |
