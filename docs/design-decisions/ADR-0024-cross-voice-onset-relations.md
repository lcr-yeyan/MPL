# ADR-0024：跨声部起音关系

状态：已采纳（V0.3.0-beta）

## 背景

数学摇滚中的复节奏需要回答“两个声部在哪些时刻同时起音”“哪些起音只属于一个声部”。完整区间重叠与通用约束求解成本高，且会把简单问题复杂化。

## 决策

1. V0.3.0-beta 的关系算子只处理**起音集合**：
   - `intersect` / `union` / `difference`；
   - `complement`；
   - `accentFollow`。
2. 坐标模式：
   - 无 grid：精确有理 onset 匹配；
   - 有 grid：先吸附到最近 grid 点，再匹配；结果事件 duration = grid。
3. 结果逻辑长度 = 两个 Rhythm 的有理共同周期；`complement` 使用显式 span。
4. 元数据规则：
   - 交集/并集/差集优先使用第一个操作数的 duration/accent/origin；
   - complement 事件 accent = 0，origin 由节点补齐；
   - accentFollow 只改 target 的 accent，保留其 onset/duration/rest 与逻辑长度。
5. 明确不处理：
   - 长音覆盖另一声部起音时的“区间吸收”；
   - 自动避免声部交叉、平行五八度；
   - 通用约束求解器。
6. 网格模式与 `quantize` 共用“最近 grid、平局向上”的规则，避免两套量化语义。

## 后果

- 可以精确表达“鼓与吉他同时起音”“只属于吉他的切分点”“鼓跟随吉他重音”。
- 关系算子不改变原始 Rhythm；结果可继续参与代数。
- 区间级关系与声部进行检查留给后续版本。
