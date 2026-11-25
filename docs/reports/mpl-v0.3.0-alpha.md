# MPL V0.3.0-alpha 开发报告：音乐编程抽象

日期：2026-11-25

## 1. 结论

V0.3.0-alpha 完成 Phase 2 收束修复，并把 MPL 从“底层音频执行”推进到“音乐创作知识的程序化表达”：

- 用户定义函数、显式参数/返回类型、局部 `let`、嵌套调用；
- 显式 Key/Meter 上下文；
- Typed HIR 求值，函数结果保留 Music IR 依赖；
- Bool / if / for / 有限集合；
- 显式种子随机原语；
- 函数缓存、稳定调用身份与调用 Trace；
- Math Rock Reference Project（MRP-001）。

全量测试 **158/158** 通过（Debug 与 Release），既有 146 项全部保留，新增 12 项函数测试。

## 2. Phase 2 收束修复

### 2.1 True Peak 判定矛盾

V0.2.0 报告中目标为 True Peak ≤ -0.5 dBTP，实测 -0.43 dBTP，却标记 satisfied。原因是判定使用了固定的 +0.1 dB 容差，但没有在状态与报告中区分。

修复：

- 响度容差与峰值容差独立：
  - `loudnessToleranceLu`（默认 0.5 LU）；
  - `peakToleranceDb`（默认 0.1 dB）。
- 新增 `MasteringStatus`：
  - `strictly-satisfied`
  - `satisfied-within-tolerance`
  - `unsatisfied`
  - `iteration-exhausted`
  - `numerical-failure`
  - `provably-unsatisfiable`
- 目标 -0.5 dBTP、实测 -0.43 dBTP → 状态 `satisfied-within-tolerance`，并产生 `mastering-peak-tolerance` 警告；不再声称严格满足。
- Trace 记录 `peakToleranceDb`、最终状态、最终 master gain、limiter ceiling、post-fader sample ceiling。

### 2.2 Limiter ceiling 与输出峰值

- `limiterCeilingDb` 是 **pre-fader**（Master Fader 之前）的采样峰值 ceiling。
- 最终 `postFaderSampleCeiling = limiterCeiling + masterGain`。
- 本次母带：
  - limiter ceiling -10.505 dB；
  - master gain +7.761 dB；
  - post-fader sample ceiling -2.744 dBFS；
  - 实测 sample peak -2.74 dBFS，与预期一致；
  - True Peak -0.43 dBTP 更高，来自 inter-sample 峰值。
- 未将 Sample Peak Limiter 宣称为 True Peak Limiter；dBTP 由分析独立验证。

### 2.3 技术债务

Step 自动化 Block 误差、通用 PDC、Sample/True Peak 差异、跨编译缓存、Meter Map / Rhythm 对象等已列入 `docs/technical-debt.md`（TD-01..TD-30），不扩大 Phase 2 范围。

## 3. 设计审查

见 `docs/reports/v0.3.0-design-review.md`：

- 复用 AST/ExprId、全局 SymbolTable、Typed HIR、Lowering、Music IR；
- 作用域栈不写入全局符号；
- 不引入 VM/LLVM；
- 函数返回 Music IR 对象 id，不扁平化；
- Trace 版本升到 4。

## 4. MRP-001 与语义保真

见 `docs/reports/mrp-001-gap-analysis.md`。

MRP-001 需求：

- 全局 7/8；
- 3/8 细胞在 7/8 内循环与相位移动；
- A/B/Reprise 结构；
- 吉他/贝斯/鼓/Lead；
- 参数化变体；
- 精确有理时间、可复现、可溯源。

已实现：

- `examples/math-rock.mpl`；
- `cell`/`riff`/`lead`/`drumBar`/`bassBar` 函数；
- `rotate` 3/8 相位；
- `random(seed,index)` 生成 lead 变体；
- 7/8 栅格鼓与贝斯；
- A 段动机在 Reprise 复用；
- `Variant`/`Seed` 参数。

明确未实现（gap）：

- Meter Map / 变拍号；
- 独立 Rhythm 对象；
- 节拍分组 2+2+3 与 Metric Hierarchy；
- 跨声部节奏对齐/冲突诊断；
- 和弦声部连接检查。

这些是 alpha 范围外的语言能力，不是隐藏近似。

## 5. 语言与实现

新增语法：

```text
fn riff(root: Pitch, variant: Int, cells: Int) -> Fragment context key, meter {
  let parts = for i in range(0, cells) {
    if i % 3 == 0 {
      rotate(cell(root, variant + 1, 104), ratio(i, 16))
    } else {
      cell(root, variant, 92)
    }
  };
  return syncopate(sequenceAll(parts), 1/16);
}
```

类型：

- Integer、Boolean、Function；
- Fragment、Curve、Duration、Position、Pitch、Int、Number、Scalar、Bool、String、Key、[T]。

求值：

- 函数参数/局部变量在 Lowering 局部作用域；
- 全局 let/curve 惰性求值；
- 函数缓存键：name + version + Key + Meter + args；
- 同参同上下文返回同一 Music IR 对象；
- 不同参数创建不同对象；Clip 实例仍独立。

控制流与预算：

- if 表达式、for、range；
- 集合 1024、循环 8192、调用深度 32、总调用 4096；
- 递归在 Sema 拒绝。

随机：

- `random(seed, index)`；
- splitmix64 派生，[0,1)；
- 无全局状态；同 seed/index 可复现。

溯源：

- `functions`：签名、上下文、版本；
- `functionCalls`：调用点、实例、深度、实参、上下文、缓存命中、对象、算子；
- `functionCache`：calls/hits/misses。

## 6. Demo 产物

### 6.1 Math Rock（MRP-001）

源：`examples/math-rock.mpl`

```text
mplc build examples/math-rock.mpl -o build/math-rock --tail 1.0
```

| 属性 | 值 |
| --- | --- |
| 拍号 | 7/8 |
| 速度 | 132 BPM |
| 段落 | A / B / Reprise，各 4 小节 |
| 轨道 | Guitar / Bass / Lead / Kit |
| Music IR | 85 objects |
| Production | 11 clips, 167 notes |
| 时长 | 19.09 s（不含尾音） |
| 音频 | 48 kHz stereo float32，973,964 frames |
| Sample Peak | -7.44 dBFS |
| True Peak | -6.51 dBTP |
| Integrated | -18.65 LUFS |
| Function calls | 22，cache hits 12 |

产物：

- `build/math-rock/math-rock.wav`
- `build/math-rock/math-rock.mid`
- `build/math-rock/math-rock.music.ir`
- `build/math-rock/math-rock.production.ir`
- `build/math-rock/math-rock.trace.json`

### 6.2 母带（Phase 2 收束）

```text
mplc master examples/production.mpl -o build/mastering --tail 1.5
```

- 目标：-14.0 ± 0.5 LUFS，True Peak ≤ -0.5 dBTP（容差 0.1 dB）。
- 结果：-14.18 LUFS，True Peak -0.43 dBTP，Sample Peak -2.74 dBFS。
- 状态：`satisfied-within-tolerance`。
- 3 轮；master gain +7.761 dB；limiter ceiling -10.505 dB pre-fader / -2.744 dB post-fader sample。
- 产物：`build/mastering/` 的 raw/mastered WAV、analysis-before/after、mastering-trace。

## 7. 测试

- 既有 146 项全部保留。
- 新增 12 项：
  - 函数与内联程序 MIDI 等价；
  - 参数/返回/作用域/return/局部类型诊断；
  - 嵌套调用与 Trace；
  - 函数缓存与对象共享；
  - 递归拒绝；
  - 控制流与随机确定性；
  - 集合/调用预算；
  - Key/Meter 上下文必需与传递；
  - 独立函数修改的无关输出稳定性；
  - MRP-001 工程编译与参数变体（Variant 1/2 产出不同 MIDI）。
- 总计 **158/158**。

说明：

- 函数缓存是单次编译内的定义共享；
- 跨编译增量 Lowering 未实现，测试验证的是“无关函数对象 id/内容在等节点数修改下保持稳定”，不是持久缓存。

## 8. 交付

- 设计审查：`docs/reports/v0.3.0-design-review.md`
- MRP 差距：`docs/reports/mrp-001-gap-analysis.md`
- 技术债务：`docs/technical-debt.md`
- ADR-0018/0019。
- 语法/语言规范更新。
- `examples/math-rock.mpl` + WAV/MIDI/Music IR/Production IR/Trace。
- 母带 Trace 增加状态与最终参数。

## 9. 范围限制

未实现：SoundBoxer Adapter、插件宿主、实时设备、GUI、完整 audio-rate modulation、空间音频、AI/LLM、情绪/Taste、通用约束求解、递归、模块、泛型、函数值、Meter Map、独立 Rhythm 对象。

## 10. 下一步（V0.3.0-beta）

1. Meter Map 与变拍号；
2. 独立 Rhythm 对象与节拍分组；
3. 跨编译函数缓存/增量 Lowering；
4. 模块/导入与稳定随机流；
5. 递归/循环的资源与终止语义。
