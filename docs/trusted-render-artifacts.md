# Trusted Render Artifact Specification（V0）

状态：V0.4.0-beta

代码：`include/mpl/lab/RenderArtifacts.hpp`、`src/lab/RenderArtifacts.cpp`

## 1. 问题

外部 WAV 可能来自任意程序、任意版本或人工编辑。若验证器直接读取任意 WAV 作为“当前候选
的渲染”，音频目标就不可信。可信渲染必须把音频与候选源程序、模块闭包、Production IR、
资产、渲染器版本和配置绑定。

## 2. Manifest

```json
{
  "format": "mpl.render-manifest",
  "version": 1,
  "sourceHash": "...",
  "moduleHash": "...",
  "productionHash": "...",
  "assetManifestHash": "...",
  "rendererVersion": "mpl-reference-audio-1",
  "configuration": "{\"sampleRate\":48000,...}",
  "audioHash": "...",
  "sourceSha256": "...",
  "moduleSha256": "...",
  "productionSha256": "...",
  "assetSha256": "...",
  "audioSha256": "..."
}
```

FNV 字段保留为快速内部身份；SHA-256 字段为实验产物内容身份。两者都不是密码学签名，
也不证明执行过程正确。

| 字段 | 计算 |
| --- | --- |
| `sourceHash` | 候选源字节 FNV-1a |
| `moduleHash` | 所有 ModuleRecord name + resolvedHash 组合 |
| `productionHash` | `ProductionIR.toJson()` 哈希 |
| `assetManifestHash` | track/instrument/asset binding 规范 JSON 哈希 |
| `rendererVersion` | 当前 reference renderer 标识 |
| `configuration` | sampleRate/blockSize/channels/tailSeconds |
| `audioHash` | WAV 字节 FNV-1a |
| `sourceSha256` / `moduleSha256` / `productionSha256` / `assetSha256` / `audioSha256` | 对应内容的 SHA-256 |

哈希是内容指纹，不是加密签名；用于排除误绑定，不用于安全认证。

## 3. 验证规则

`verifyRenderManifest` 重新计算全部字段并逐项比较：

- source 改变 → 失败；
- 模块依赖改变 → moduleHash 改变 → 失败；
- Production 改变 → productionHash 改变 → 失败；
- 资产绑定改变 → assetManifestHash 改变 → 失败；
- renderer 版本/配置改变 → 失败；
- WAV 字节改变 → audioHash 改变 → 失败。

任一项不匹配，音频观察不得作为当前候选结果。

## 4. 观察绑定

```cpp
ObservationOptions options;
options.audioPath = "...";
options.expectedAudioHash = manifest.audioHash;
```

`observeCompilation` / `observeFlatEvents` 在读取 WAV 前比较 hash；不匹配时
sample_peak / true_peak / integrated_loudness 全部 `unverifiable`，reason 为
trusted render 不匹配。

## 5. Harness

Harness **不默认信任** submission 的 `--audio`：

- 只有 manifest 与 candidate 源/编译/配置/音频全部匹配，音频才可用于音频任务；
- 外部导入 WAV 若没有 manifest，只能作为未验证产物，音频观察为 unverifiable；
- `renderTrusted(source, sourcePath, config, wavPath, manifestPath)` 完成
  Frontend → Compiler → ReferenceAudioBackend → WAV → manifest，manifest 绑定实际
  写出的音频字节；
- 自动渲染命令：

```text
mpl_lab render <program.mpl> <out.wav>
```

生成 `<out.wav>.manifest.json`。`mpl_lab verify` 的 `--audio` 必须同时提供
`--manifest`，否则拒绝。

## 5.1 SHA-256

`sourceSha256` / `moduleSha256` / `productionSha256` / `assetSha256` /
`audioSha256` 使用 FIPS 180-4 SHA-256。FNV 字段作为快速内部身份保留。两者都只是内容
指纹，不是数字签名，也不证明执行过程正确。

## 6. 可信与未验证

| 类别 | 含义 |
| --- | --- |
| Trusted | 由 Harness/编译器控制执行生成，manifest 全部匹配 |
| Imported-unverified | 外部 WAV，无匹配 manifest；不得计为当前候选渲染 |
| Mismatched | 有 manifest 但 hash 不匹配；拒绝 |

## 7. 测试

- `RenderManifestBindsSourceProductionAndAudio`：manifest 往返、音频字节改变失败；
- `ObservationRejectsUntrustedAudioHash`：期望 hash 不匹配时音频观察 unverifiable；
- `TaskVerifierReportsUnverifiableAudioWithoutWav`：无 WAV 不伪造 pass。

## 8. 限制

- FNV 不是密码学签名；
- 不实现资产文件字节级 manifest（当前绑定 asset id/provider）；
- 不实现渲染缓存；
- 不自动证明 WAV 由 manifest 对应的进程生成；manifest 与受控执行步骤必须一起使用。
