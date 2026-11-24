# ADR-0007：Definition 与 Instance 分离及稳定身份

状态：已采纳（V0.0.3）

## 背景

同一动机需要被多个段落复用，并分别拥有独立的演奏表情。V0.0.2 的 Production IR 只有 clip，没有实例身份与参数身份，trace 也难以定位具体播放。

## 决策

1. Music IR 对象保持定义身份 `ObjectId`。
2. Production IR 的每个 clip 获得 `InstanceId`，按 lowering 顺序确定性分配。
3. 音符播放身份为 `NoteInstanceId = (InstanceId, noteIndex)`，trace 输出 `instance:index`。
4. 自动化参数使用名称型 `ParameterId`（如 `track:Piano:volume`），不随轨道索引变化。
5. `PerformanceExpression` 挂在 clip 实例上，包含 velocity / timing / gate / pan 四条相对时间曲线。

## 后果

- 同一对象可多次实例化并拥有不同表情。
- 参数 id 在重排轨道后保持稳定；实例 id 在重排后可能变化，这是已记录的限制。
- Production IR 仍是快照式 clip；跨层增量编译尚未实现。
