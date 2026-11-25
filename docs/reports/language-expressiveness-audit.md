# Language Expressiveness Audit（V0.5.0-alpha）

日期：2026-11-25

本审计只记录事实与工程判断，不以“模型是否容易使用”作为增删语言功能的依据。

| 能力 | 当前支持 | 语义限制 | 实际生产需求 | 可替代实现 | 引入成本 / 类型 / 编译器 / 运行时 | 优先级 |
| --- | --- | --- | --- | --- | --- | --- |
| 基础类型 | Int/Number/Bool/String/Pitch/Duration/Position/Key/Fragment/Rhythm/Chord/Harmony/Voicing | 标量为主，无用户定义结构 | 高：所有程序 | 现有 stdlib 函数 | 低/中；类型系统小改；前端中改；运行时无 | P2 |
| 集合 | `[T]` 字面量、range/length、for 遍历 | **无索引**；无 map/set；无切片 | 高：和弦音、声部列表、模式 | cycle/zip/voicings 封装 | 中；类型/前端中改；运行时无 | **P0** |
| 参数化类型 | 仅 `[T]` | 无用户泛型、无类型参数约束 | 中：通用算法复用 | 具体类型重复 | 高；类型系统/前端大改 | P3 |
| 函数/高阶 | 纯函数、参数/返回、context、有限深度 | 无高阶函数/闭包/递归 | 高：主题变换、复用 | 内建/复制 | 高；类型/编译器/运行时都受影响 | P2 |
| 控制流 | for、if | 无 while、早 return、break | 中：复杂生成 | range/for 重组 | 中；需终止语义 | P2 |
| 模块/包 | 文件 import/export、私有名 mangle | 无命名空间/版本/包管理 | 中：复用 | 单文件 | 中；链接器已有基础 | P2 |
| 音乐时间 | 精确 Rational、MeterMap、变拍号、bars/beats | Tempo 只有初始事件，无完整 tempo map | 高 | 现有 | 中/高；运行时/音频 | P1 |
| 音乐结构 | Section/Track/Clip、Fragment/Rhythm/Harmony/Voicing | 无 Tie；Section 非一等值；无结构复用变量 | 高 | sequence/repeat | 中；Music IR/音频 | **P0**（Tie） |
| 音乐关系 | onset 集合、声部连接、共同音、音域、平行分析 | 区间级关系不足；无通用关系代数 | 高 | 显式分析函数 | 中；分析层 | P1 |
| 状态/信号 | 曲线、自动化、处理器图 | 无有状态节点/反馈变量 | 中 | 现有 graph | 高；运行时 | P2 |
| 音乐资产 | instrument/sample 名称 | 无任意文件 I/O，资产绑定有限 | 中 | ReferenceAssetLibrary | 中；运行时/安全 | P2 |
| 生产图 | processors/buses/master/automation | 无动态图构造 | 高 | 静态声明 | 中；图验证 | P1 |
| 契约/验证 | 外部 Task/Observation/Frame/Knowledge checks | 语言内无 `contract` 语法；错误/证据在工具层 | 高 | C++/JSON task | 中；不需要核心语法 | P1 |
| 资源/执行 | 事件/调用/循环上限、确定性随机 | 无并发/长任务；无资源预算语法 | 高 | 工具预算 | 中；运行时 | P1 |

## 关键结论

1. **列表索引缺失**是实际生产表达能力的 P0 缺口：voicing/pattern 任务必须通过
   `cycle`/`zip` 或复制代码绕过；V0.4 Learnability 中模型使用 `pitches[0]` 失败，
   但这是真实语言限制，不是模型偏好。
2. **Tie 缺失**使跨周期持续音只能截断/retrigger，属于音乐结构 P0/P1 缺口。
3. **高阶函数/递归**不是当前生产阻塞项；在未建立终止/资源语义前不引入。
4. **完整 Tempo Map** 是时间表达的真实限制，优先级高于模型专用语法。
5. 语言内 `contract` 语法并非必要：外部 Task/Knowledge Contract 已能验证，且不污染核心。

## 建议进入 V0.6 的候选

- P0：列表索引/切片（受限、不可变、越界诊断）；
- P0：Tie/持续音组；
- P1：区间级音乐关系；
- P1：完整 Tempo Map；
- P2：高阶函数（先做终止/预算语义）。

本阶段不实施这些语言扩展。
