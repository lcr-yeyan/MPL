# ADR-0035：核心/知识/实验分层与无模型构建

状态：已采纳（2026-11-25）

## 背景

战略修正要求 MPL 保持独立语言/编译器/运行时定位。审计发现 `mpl_core` 曾聚合
ModelAdapter、Harness、Repair、Statistics、KnowledgeResolver、AdaptiveContext，
并链接 `winhttp`。这使核心静态库带有模型供应商网络依赖。

## 决策

1. 拆分为：
   - `mpl_core`：语言、编译器、运行时、Observation/Delta/Frame/Trusted；
   - `mpl_knowledge`：Knowledge Registry、TaskRequirements、KnowledgeVerifier；
   - `mpl_experiment`：ModelAdapter、Harness、Repair、Statistics、Resolver、AdaptiveContext。
2. `winhttp` 只在 `mpl_experiment` 链接。
3. 新增 `MPL_BUILD_EXPERIMENTAL_AI`（默认 ON）。OFF 时：
   - 不生成 `mpl_experiment`/`mpl_lab`；
   - `mplc`、`mpl_tests`、`mpl_knowledge` 仍可构建；
   - 测试排除实验专用文件。
4. 新增 CTest `mpl_architecture_independence`，检查直接/传递链接：
   - core 不得链接 knowledge/experiment；
   - knowledge 不得链接 experiment；
   - mplc 不得链接 knowledge/experiment。
5. 保留模型实验代码与记录，不删除。

## 结果

- ON：331/331 测试；
- OFF：304/304 测试；core-only `mplc` 可完成 MRP 渲染；
- `mpl_core` 无 `winhttp`/model target；
- `mpl_lab` 只在实验层构建。

## 后果

- 实验功能需要显式开启构建；
- 核心发布二进制更小、依赖更少；
- 模型研究仍可独立进行；
- 未来新增实验模块必须遵守同一依赖方向。
