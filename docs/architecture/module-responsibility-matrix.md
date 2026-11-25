# Module Responsibility Matrix

日期：2026-11-25  
适用：V0.5.0-alpha / 模型无关架构

## 1. 目标

明确模块职责、公开 API、依赖、运行时要求、模型/网络依赖、确定性与测试覆盖；
验证：

- `mpl_core` 不依赖 `mpl_knowledge` 或 `mpl_experiment`；
- `mpl_knowledge` 不依赖 `mpl_experiment`；
- 核心目标不通过传递依赖引入模型供应商网络库。

## 2. 矩阵

| Module | Responsibility | Public API | Direct Dependencies | Transitive | Runtime Requirements | Model Dependency | Network Dependency | Determinism | Test Coverage | Layer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `mpl_core/core` | Rational、MeterMap、TimeTypes | `mpl/core/*.hpp` | 无 | 无 | 无 | 无 | 无 | 确定 | core/meter 48 tests | Core Language |
| `mpl_core/music` | Pitch/Key/Chord/Harmony/Rhythm/Fragment/Transform | `mpl/music/*.hpp` | core | 无 | 无 | 无 | 无 | 确定 | rhythm/harmony/relations 66+ | Core Language |
| `mpl_core/frontend` + `Compiler` | Lexer/Parser/Sema/Lowering/诊断/函数缓存 | `compiler.hpp`, `frontend/*` | core/music | music | 无 | 无 | 无 | 确定 | frontend/lowering/semantics 36 | Compiler |
| `mpl_core/ir` | Music/Production IR、对象、模块哈希 | `mpl/ir/*` | core/music | frontend | 无 | 无 | 无 | 确定 | ir/modules 16 | Compiler |
| `mpl_core/temporal` + `passes` | 时间域、Pass 管理 | `mpl/temporal`, `passes` | core/ir | — | 无 | 无 | 无 | 确定 | temporal 16 | Compiler |
| `mpl_core/backend` | MIDI 后端 | `mpl/backend/*` | ir | — | 文件系统 | 无 | 无 | 确定 | backend 9 | Runtime |
| `mpl_core/audio` + `mastering` | 48k WAV、处理器、分析、mastering | `mpl/audio/*`, `mastering` | ir/backend | — | CPU、文件系统 | 无 | 无 | Reference backend 确定；宿主后端不保证 | audio/signal/analysis 48 | Runtime |
| `mpl_core/observe` + `analysis` | Observation Registry、Semantic Delta | `observe/*`, `analysis/*` | ir/audio | — | 无/音频 | 无 | 无 | 确定 | observe/delta 8 | Semantic Infrastructure |
| `mpl_core/lab` 语义部分 | Task、Flat baseline、Frame、Render Manifest、Trusted Execution | `lab/Task`, `FlatEvents`, `RenderArtifacts`, `TrustedExecution` | observe/audio | — | 文件系统/CPU | 无 | 无 | 确定 | lab/render/benchmark 47 | Semantic Infrastructure |
| `mpl_knowledge` | Registry、TaskRequirements、KnowledgeVerifier | `knowledge/Knowledge`, `TaskRequirements`, `KnowledgeVerifier` | core | core | 无 | 无 | 无 | 确定 | knowledge 6 core tests | Semantic Infrastructure |
| `mpl_experiment` | ModelAdapter、Harness、Repair、Statistics、Resolver、AdaptiveContext | `lab/ModelAdapter(s)`, `Harness`, `Repair`, `Statistics`, `knowledge/Resolver`, `AdaptiveContext` | core/knowledge | core/knowledge | 网络仅 model adapter；否则本地 | **可选** | **仅 OpenAI adapter** | 本地部分确定；模型输出非确定 | lab 17、repair 5、stats 2、context 3 | Experimental AI |
| `mpl_demo` | 示例/端到端包装 | `examples/CoreDemo` | core | core | 无 | 无 | 无 | 确定 | core demo | Toolchain |
| `mplc` | 编译/检查/AST/report/音频构建 | CLI | demo/core | core | 文件系统 | 无 | 无 | 确定 | golden/benchmark | Toolchain |
| `mpl_lab` | 观察/验证/实验/知识/上下文 CLI | CLI | demo/knowledge/experiment | 全层 | 文件系统；model 时网络 | 仅 model 命令 | 仅 model 命令 | 本地确定 | lab/repair/stats | Toolchain（可选） |
| `mpl_tests` | 回归/黄金/知识/实验测试 | 无 | demo/core/knowledge/experiment | 全层 | 文件系统 | 仅 model/repair 测试 | 不实际联网 | 确定（网络测试用 fake） | 346 tests | Toolchain |
| `lcp/` | 冻结实验上下文 | 数据文件 | 无 | 无 | 无 | 无 | 无 | 内容哈希 | 实验记录 | Experimental data |
| `knowledge/v1` | 知识 catalog/schema/manifest | 数据 | 无 | 无 | 无 | 无 | 无 | contentHash | knowledge tests | Semantic data |

## 3. 验证命令

```text
cmake -S . -B build -DMPL_BUILD_EXPERIMENTAL_AI=ON
cmake -S . -B build-core-only -DMPL_BUILD_EXPERIMENTAL_AI=OFF
cmake --build build-core-only --config Debug --target mplc mpl_tests
build-core-only\Debug\mpl_tests.exe
build-core-only\Debug\mplc.exe build examples\math-rock.mpl -o build-core-only\math-rock
```

- `mpl_core_tests`：346/346（ON）；319/319（OFF，排除实验测试）；
- `mpl_architecture_independence`：CTest，检查直接/传递链接；
- core-only 构建不生成 `mpl_experiment`，不链接 `winhttp`。

## 4. 依赖规则

```text
mpl_core           -> (none)
mpl_knowledge      -> mpl_core
mpl_experiment     -> mpl_core, mpl_knowledge, winhttp (Windows)
mpl_demo           -> mpl_core
mplc               -> mpl_demo
mpl_lab            -> mpl_demo, mpl_knowledge, mpl_experiment
mpl_tests          -> all
```

禁止：

- `mpl_core -> mpl_knowledge/mpl_experiment/winhttp`；
- `mpl_knowledge -> mpl_experiment`；
- `mplc -> knowledge/experiment`；
- 通过 interface target 间接引入实验层。

## 5. 残留说明

- `Task` 支持 Flat Event baseline 验证；这是表示基线，不是模型专用；
- `mpl_lab` 聚合所有层，是工具，不是核心；
- `mpl_tests` 链接实验层以测试它，但不改变生产二进制依赖；
- `mpl_experiment` 的 model adapter 可按构建选项完全关闭。
