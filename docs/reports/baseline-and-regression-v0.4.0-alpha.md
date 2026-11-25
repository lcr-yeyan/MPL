# Baseline and Regression Report（V0.4.0-alpha）

日期：2026-11-25

## 1. 回归结论

| 版本 | 测试数 | 结果 |
| --- | --- | --- |
| V0.3.0-rc | 277 | 277/277 |
| V0.3.0 Final | 295 | 295/295 |
| 新增 | 18 | 全部通过 |
| 删除/跳过 | 0 | 无 |

RC 的 277 项全部保留；没有以“固定测试数量”替代语义覆盖。

## 2. 文件分布（Final）

| 文件 | 数量 |
| --- | --- |
| test_core | 25 |
| test_audio | 13 |
| test_signal | 20 |
| test_analysis | 15 |
| test_temporal | 16 |
| test_functions | 15 |
| test_frontend | 15 |
| test_meter | 23 |
| test_rhythm | 29 |
| test_harmony | 24 |
| test_relations | 13 |
| test_benchmark | 25 |
| test_backend | 9 |
| test_lowering | 8 |
| test_ir | 6 |
| test_golden | 6 |
| test_semantics | 13 |
| test_modules | 10 |
| test_observe | 3 |
| test_lab | 7 |
| **合计** | **295** |

## 3. 新增审计回归

| 测试 | 覆盖 |
| --- | --- |
| `ContinuationWithoutPredecessorIsRejected` | 孤立 continuation 诊断 |
| `RhythmCombineDropsContinuation` | 切分尾段不产生新 attack |
| `VoiceLeadSearchCompletenessIsExplicit` | 预算耗尽与完整搜索区分 |
| `BenchmarkCompilerReport` | 机器可读编译报告/版本/哈希/phase |
| `BenchmarkMrpExtentAudit` | logical/content/tail/render/frames 关系 |
| `ModuleTransitiveHashChangesWithDependency` | 传递依赖改变 resolvedHash |
| `MeterMapNegativeBarsAreLinearAndAsymmetric` | 正负 bars 非对称 |
| `HarmonySustainedNoteKeepsOnsetChord` | 跨和声持续音不自动切分 |
| `InstrumentCapabilityGuitarBoundaries`（增强） | 每个弦/品位置、最高音可达 |

## 4. 新增基础设施测试

| 测试 | 覆盖 |
| --- | --- |
| `ObservationRegistryDeclaresAllRequiredObservations` | 16 项注册表元数据 |
| `ObservationValuesFromMrpAreExactAndUnverifiableWhereRequired` | 精确/不可验证分类 |
| `ObservationSetIsDeterministic` | 观察输出确定性 |
| `JsonValueParsesTaskShape` | 任务 JSON 子集 |
| `TaskVerifierAcceptsBaselineAndLocalEdit` | 验证器接受正确 baseline/候选 |
| `TaskVerifierRejectsUnchangedCandidateForEditTask` | 未修改候选失败 |
| `TaskVerifierRejectsPreserveViolation` | preserve 违反失败 |
| `TaskVerifierReportsUnverifiableAudioWithoutWav` | 音频目标不伪造 pass |
| `TaskParserRejectsUnknownObservation` | 未知观察诊断 |
| `OfflineHarnessDoesNotClaimModelSuccess` | 离线运行不报告模型成功率 |

## 5. MRP-001 基线

| 项 | 值 |
| --- | --- |
| logical extent | 10 whole |
| content extent | 9.875 whole |
| processor tail | 0 |
| instrument release（最大） | 0.25 s |
| render tail | 1.0 s |
| rendered extent | 19.204545 s |
| frames @48k | 921,818 |
| music objects | 144 |
| modules | 4 |
| tracks/clips/notes | 4 / 14 / 247 |
| MIDI meter events | 3（tick 0 / 6720 / 11520） |
| function definitions/calls | 21 / 63 |

## 6. MPL-Bench-001 验证器基线

未调用模型。

| 候选 | 结果 |
| --- | --- |
| baseline on 01–07, 11 | pass |
| baseline on 08 | fail（需要节奏替换） |
| baseline on 09 | fail（需要和声替换） |
| baseline on 10 | fail（需要局部音高编辑） |
| baseline on 12 | unverifiable（无 WAV） |
| `candidates/rhythm-replacement.mpl` on 08 | pass |
| `candidates/harmony-replacement.mpl` on 09 | pass |
| `candidates/local-edit.mpl` on 10 | pass |
| `bench/experiments/offline-example.json` | modelGenerated=false；observedSuccessRate=null |

这些是基础设施回归，不是模型生成结果。

## 7. 构建与命令

```text
cmake -S . -B build
cmake --build build --config Debug --target mpl_tests mplc mpl_lab
build\Debug\mpl_tests.exe

cmake --build build --config Release --target mpl_tests mplc mpl_lab
build\Release\mpl_tests.exe

mplc report examples/math-rock.mpl
mpl_lab validate bench/tasks
mpl_lab verify bench/tasks/01-basic-notes.json bench/baselines/basic.mpl
mpl_lab run bench/experiments/offline-example.json
```

## 8. 未执行

- 模型 API 调用；
- 真实模型 Token/成功率；
- A 组 MIDI JSON 的 MPL 观察验证；
- 自动修复循环。

这些未执行项不作为通过项报告。
