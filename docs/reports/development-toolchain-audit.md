# Development Toolchain Audit（V0.5.0-alpha）

日期：2026-11-25

本阶段只审计与规划，不实现全部工具。

## 1. 现有基础设施

| 设施 | 现状 |
| --- | --- |
| Lexer/Parser | `src/frontend`，带 SourceSpan、诊断码 |
| AST | `mplc ast <file>` 输出结构化 AST JSON |
| 类型检查/Sema | 完整前后端管线，诊断码 |
| Lowering | 类型化 AST → Music IR；**没有独立 HIR** |
| Music IR | `include/mpl/ir`，对象/片段/生产/时间 |
| Production IR | 轨道、Clip、处理器、自动化、资产 |
| Trace | `Compilation` 记录函数调用、节点、上下文、缓存 |
| Report | `mplc report <file>` 输出功能/资源/模块/布局 |
| Diagnostics | 结构化 code/message/span，`check --json` |
| 观察/验证 | `mpl_lab observe/verify/manifest/check-manifest` |
| 音频分析 | `mplc analyze`，峰值/LUFS/true peak |
| 测试 | C++ 单元/黄金测试 + task/benchmark 验证；无 MPL 用户测试 DSL |
| 项目系统 | CMake + `project` 块；无 MPL 项目清单 |
| 包管理 | 本地 `import "path"`；无 registry/version/lock |

## 2. 工具路线

| 工具 | 可复用基础设施 | 缺失能力 | 依赖 | 复杂度 | 普通开发者价值 | 优先级 |
| --- | --- | --- | --- | --- | --- | --- |
| Language Server | Parser、AST、Sema、Diagnostic、SourceSpan | JSON-RPC/LSP、文档同步、符号索引、增量解析、补全/跳转 | 现有前端 + 位置映射 | 高 | 很高（编辑器集成） | **P1** |
| Formatter | Lexer span、AST | 保留注释/空白的 lossless CST、稳定格式规则、幂等测试 | Lexer/trivia | 高 | 高（协作/可读性） | P2 |
| Linter | AST/Sema/诊断 | 规则引擎、可配置规则、quick-fix | AST/Sema | 中 | 高（早期错误） | **P1** |
| Inspector | `ast`/`report`/IR/Trace | 交互式对象图、节点来源、diff 视图 | 现有 IR/Trace | 中 | 高（调试音乐结构） | **P1** |
| Debugger | Trace、IR、SourceSpan | 断点、单步、求值、实例/自动化检查 | 运行时/IR | 高 | 中高 | P3 |
| Test Framework | C++ 测试、golden、Task Verifier | MPL 测试 DSL、断言、fixture、并行 runner | Parser/Compiler | 中 | 高（用户回归） | P2 |
| Documentation Generator | AST doc comments? | 注释保留、doc comment 语义、HTML/Markdown | Lexer/Parser | 中 | 中高 | P2 |
| Project System | `project`、CMake | MPL manifest、依赖图、构建缓存 | 前端/链接器 | 中 | 高 | P2 |
| Package Manager | 本地 import/模块哈希 | registry、版本约束、lock、校验 | 项目系统 | 高 | 中（生态） | P3 |
| Profiler | `analyze`、Trace | 每节点/算子计时、内存、音频块统计 | IR/运行时 | 中 | 中高（性能） | P2 |

## 3. 关键判断

- **LSP 和 Linter 优先**：它们直接复用 Parser/AST/Sema，对普通开发者价值最高；
- **Formatter 需要 lossless CST**：当前 AST 不保留全部 trivia，不能假装已有；
- **Inspector 是现有 `report`/`trace` 的自然延伸**，不应另写解析器；
- **Debugger 成本高**，在 IR/Trace 稳定后再做；
- **Package Manager 不应先于 Project System**；
- 所有工具必须能离线工作，不依赖模型。

## 4. 实现路线

### Phase T1（P1）

- LSP：document sync + diagnostics + hover + go-to-definition；
- Linter：未使用变量、越界常量、不可达/空 sections、资源上限预警；
- Inspector：`mplc inspect --json` 统一 AST/IR/Trace 视图。

### Phase T2（P2）

- Formatter：先做保守格式化，保留注释；
- 用户测试 DSL：`test`/`expect` 或外部 JSON fixture；
- Doc generator：从 doc comment 生成 Markdown；
- Project manifest：`mpl.project.json` + lock；
- Profiler：编译/渲染阶段计时。

### Phase T3

- Debugger：基于 IR 的断点/单步；
- Package registry。

## 5. 明确不做

- 不复制一套 Parser/Sema 给工具；
- 不为模型生成专用 LSP/补全；
- 不把 Prompt/上下文文件当项目依赖；
- 不在本阶段实现上述工具。
