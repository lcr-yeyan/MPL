# ADR-0013：资产解析、版本与缺失诊断

状态：已采纳（V0.2.0-alpha）

## 背景

Production IR 只携带 `AssetSlot` / `AssetBinding` 的符号信息；音频执行需要具体音源。若解析失败时静默静音，音乐语义会被悄悄丢失。

## 决策

1. 引入后端无关 `AssetResolver` 接口，把 TrackIR + AssetBinding 解析为 `InstrumentAsset`。
2. `InstrumentAsset` 固化 id、version、振荡器/包络/增益/采样；计划构建时解析一次。
3. Reference Resolver 支持：
   - 已知符号音色族（piano/pad/bass/lead/drum）；
   - `file:` 或 `.wav` 绑定加载 PCM16/float32 WAV；
   - 未知资产返回 `audio-unknown-asset`。
4. 缺失绑定/资产/文件、WAV 读取失败均为错误，不静默替代。
5. 同一 Production IR 可替换 Resolver，音符/时间结构不变；测试验证 sine/saw 替换。
6. 情绪/审美标签不进入 Resolver 的硬编码选择规则。

## 后果

- 音色替换与资产缺失可测试、可诊断。
- 资产版本固化在 Audio Execution Plan，便于复现。
- 未来 SoundBoxer Adapter 可实现同一接口，映射 IAudioSource/插件。
