# ADR-0005：实验性高级算子保持纯函数与最小语义

状态：已采纳（V0.0.2）

## 背景

Phase 0.5 需要验证高级音乐算子的语义接口、上下文依赖与可追溯性，但不引入约束求解器、生成模型或风格规则。

## 决策

1. 新增三个算子：
   - `diatonicTranspose(fragment, scaleSteps)`；
   - `harmonize(fragment, degreeOffsets, velocity)`；
   - `syncopate(fragment, displacement)`。
2. 算子实现位于 `music/Harmony`，Music IR 只负责节点、上下文与溯源，不内嵌算法。
3. 算子行为保持简单、确定、可测试，并在规范中明确其非目标（和声功能、声部进行、风格、张力）。
4. 生成音以算子对象为 origin；参数写入 Music IR JSON 与 trace。
5. 不为未来算法预留空接口或伪参数。

## 后果

- 高级算子进入编译器的最小闭环，但不会被误认为成熟的作曲算法。
- 未来 stdlib、C++ 原生算子或外部服务可以在相同节点/上下文模型上实现。
- 若语义升级，需要版本化 IR 与黄金输出。
