# ADR-0010：文本程序的稳定实例身份

状态：已采纳（V0.1.0）

## 背景

V0.0.3 的 `InstanceId` 按 lowering 顺序分配。文本程序编辑后，顺序变化会导致实例身份漂移，影响增量编译与外部引用。

## 决策

1. 文本实例 key 由语义名称组成：
   `track:<track>|section:<section>|play:<n>|clip:<clip>`。
2. `n` 是同一 section 在 arrangement 中的出现序号，从 0 开始。
3. key 通过 FNV-1a 映射为 32-bit `InstanceId`，由 Lowering 写入 `ClipSpec.instanceId`。
4. 冲突检测失败报 `sema-instance-id-collision`，不静默覆盖。
5. 不使用源码行号、声明索引或 lowering 顺序作为长期身份。
6. `ObjectId` 仍是单次编译内的 IR 身份；`ParameterId` 仍是名称型。

## 影响

- 重命名 track/section/clip 会改变实例身份。
- 在 arrangement 中插入同一 section 的更早播放，会改变后续同 section 的 `n`。
- 插入其他 section、编辑音符或曲线不会改变已有实例身份。
- 未提供显式 id 的 C++ 构造仍使用顺序分配；前端构建全部使用稳定 key。

## 后果

- 源程序的实例身份可跨编译稳定，且不依赖行号。
- 重命名/重复 section 的语义变化被明确记录，而不是隐式漂移。
- 未来可增加显式 `as <name>` 语法进一步稳定重复播放的身份。
