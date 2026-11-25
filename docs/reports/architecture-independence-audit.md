# Architecture Independence Audit（V0.5.0-alpha）

日期：2026-11-25

## 1. 模块分类

| 层 | 目标/文件 | 内容 | 模型依赖 |
| --- | --- | --- | --- |
| Core Language | `include/mpl/core`, `music` | Rational、MeterMap、Pitch、Chord、Rhythm、Harmony | 无 |
| Compiler | `src/frontend`, `src/Compiler.cpp` | Lexer/Parser/Sema/Lowering/IR | 无 |
| Runtime | `src/audio`, `src/backend`, `src/mastering` | 离线渲染、MIDI、母带 | 无 |
| Standard Library | `stdlib/*.mpl` | 节奏/和声/排列函数 | 无 |
| Semantic Infrastructure | `src/observe`, `analysis`, `lab/Task`, `FlatEvents`, `RenderArtifacts`, `TrustedExecution` | 观察、差分、Frame、任务验证、可信渲染 | 无 |
| Development Toolchain | `tools/mplc`, CMake, tests | 编译、检查、测试 | 无 |
| Experimental AI | `tools/mpl_lab`, `mpl_experiment` | model adapter、harness、repair、context resolver/composer、statistics | 有（可选） |
| Knowledge | `mpl_knowledge` | Registry、TaskRequirements、KnowledgeVerifier | 无 |

## 2. 构建依赖

```text
mpl_core
  <- mpl_demo
  <- mplc (only demo/core)
  <- mpl_knowledge
       <- mpl_experiment
            <- mpl_lab
            <- mpl_tests
```

- `mpl_core` **不再**包含 `ModelAdapters`、`Harness`、`Repair`、`Statistics`、
  `KnowledgeResolver`、`AdaptiveContext`；
- `winhttp` 只链接到 `mpl_experiment`；
- `mplc` 不链接 knowledge/experiment；
- `mpl_knowledge` 不依赖模型 API；
- `mpl_experiment` 是唯一可选实验层。

## 3. 七项检查

### 3.1 核心编译器是否存在模型相关依赖

否。`src/Compiler.cpp`、`src/frontend`、`src/music`、`src/ir`、`src/audio`、
`src/backend` 不 include `mpl/lab`、`mpl/knowledge` 或 model adapter。
CMake 审计修复了此前 `mpl_core` 打包 model adapter + winhttp 的问题。

### 3.2 核心运行时是否完全离线

是。`mplc` 的编译/MIDI/WAV/分析路径无网络调用；可信渲染只写本地文件。
模型网络调用只在 `OpenAiCompatibleAdapter`（experiment 库）中。

### 3.3 知识库是否独立于模型调用

是。`mpl_knowledge` 只依赖 `mpl_core`；33 单元、31 verified 的验证完全在本地执行。
没有 LLM 检索或远程知识服务。

### 3.4 验证系统是否服务任意开发者

是。Task Verifier、Observation Registry、Semantic Delta、Frame Contract、
Render Manifest 均为普通 C++ API/CLI；`mpl_lab verify` 不需要模型。

### 3.5 实验模块是否反向污染语言设计

未发现。V0.5 未改 Grammar/类型/核心语义；知识/上下文均在外围。
但 `Task` 中的 `verifyFlatTaskFile` 仍耦合 Flat 表示；Flat 是表示基线，不是模型专用，
记录为可接受残留。

### 3.6 不必要的模型专用抽象

- `KnowledgeResolver` / `AdaptiveContext` 是模型上下文实验，已移入 `mpl_experiment`；
- `LCP` 文件不是编译器输入；
- `practice` 知识不进入硬约束；
- `ModelAdapter` 不在 core；
- 仍存在的模型相关 CLI（`mpl_lab model`）只在 experiment 层。

### 3.7 长期独立价值

保留为核心/语义基础设施：

- Knowledge Registry（音乐计算知识、标准库语义索引）；
- Knowledge Contracts（函数/音乐计算验证）；
- Observation Registry；
- Semantic Delta；
- Frame Contract；
- Trusted Execution / Render Manifest；
- Task Verifier。

独立实验价值：

- LCP / Adaptive Context / Resolver；
- Model Adapter / Repair / Statistics；
- six-arm harness。

## 4. 发现与修复

| 发现 | 处理 |
| --- | --- |
| `mpl_core` 曾包含 ModelAdapters/Harness/Repair 并链接 winhttp | 拆出 `mpl_experiment`；core 无 winhttp |
| Resolver/AdaptiveContext 与 Registry 同库 | 移到 `mpl_experiment`；`mpl_knowledge` 只保留语义知识 |
| `mpl_lab` 是混合 CLI | 保留为开发/实验工具，不属于 compiler/runtime |

## 5. 残留耦合

- `mpl_knowledge` 依赖 `lab/Task`（任务契约），属于语义基础设施；
- `Task` 支持 Flat Event baseline，属于表示对照而非模型专用；
- `mpl_lab` 同时链接 core/knowledge/experiment，是工具层聚合，不污染核心；
- LCP/Context 文件仍在仓库根，但不在 compiler 构建。

## 6. 结论

核心编译器与运行时无模型依赖；知识/契约可独立运行；模型/上下文实验位于可选的
`mpl_experiment`。V0.5 的“独立语言/编译器/运行时”定位保持不变。

## 7. 构建验证（新增）

| 配置 | 构建 | 测试 | 结果 |
| --- | --- | --- | --- |
| 默认（实验层 ON） | `mplc`、`mpl_lab`、`mpl_tests` | 346/346 | 通过 |
| 核心-only（`-DMPL_BUILD_EXPERIMENTAL_AI=OFF`） | `mplc`、`mpl_tests`（无 `mpl_lab`/`mpl_experiment`） | 319/319 | 通过 |
| 架构 CTest | `cmake/verify_dependencies.cmake` | 直接/传递链接检查 | 通过 |

核心-only 验证：

- `mplc build examples/math-rock.mpl` 正常完成；
- 144 objects、4 tracks、14 clips、247 notes、921,818 帧；
- 构建产物不包含 `mpl_experiment`/`winhttp`；
- 知识 Registry/Verifier 测试仍执行（6 个核心知识测试）。

命令：

```text
cmake -S . -B build-core-only -DMPL_BUILD_EXPERIMENTAL_AI=OFF
cmake --build build-core-only --config Debug --target mplc mpl_tests
build-core-only\Debug\mpl_tests.exe
```

模块职责见 `docs/architecture/module-responsibility-matrix.md`；决策见
ADR-0035。