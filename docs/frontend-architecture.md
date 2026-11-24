# MPL V0.1.0 前端架构

## 1. 目标

把 `.mpl` 源文件编译到现有 Core：

```text
source
  → Lexer → Token[]
  → Parser → AST
  → Resolver → 名称绑定 + SymbolTable
  → TypeChecker → Typed HIR
  → Lowering → Project(MusicIR + TemporalIR + TrackSpec/Automation)
  → Compiler → Compilation
  → MidiBackend → MIDI + 诊断
```

Parser 不执行音乐逻辑；只有 Typed HIR 通过后才构造 Core 对象。

## 2. 模块

| 文件 | 职责 |
| --- | --- |
| `frontend/Source.hpp` | SourceSpan、行/列定位 |
| `frontend/Token.hpp` | TokenKind、Token |
| `frontend/Lexer.hpp/.cpp` | 词法、注释、字符串、诊断 |
| `frontend/Ast.hpp/.cpp` | AST arena、AST JSON |
| `frontend/Parser.hpp/.cpp` | 递归下降、错误恢复 |
| `frontend/Symbols.hpp/.cpp` | SymbolTable、作用域、名称解析 |
| `frontend/Diagnostic.hpp/.cpp` | 人类/JSON 诊断、SourceSpan |
| `frontend/Hir.hpp` | Typed HIR：AST + 类型/符号/常量注解 |
| `frontend/Sema.hpp/.cpp` | 类型、单位、时间域、自动化、section 检查 |
| `frontend/Lowering.hpp/.cpp` | HIR → Core Project、稳定实例 id |
| `frontend/ParameterRegistry.hpp/.cpp` | 参数单位/范围/作用域/模式 |
| `frontend/Frontend.hpp/.cpp` | 对 CLI/测试的统一入口 |

## 3. 诊断

```cpp
struct FrontendDiagnostic {
    std::string code;              // 稳定错误码
    Severity severity;             // error/warning/note
    SourceSpan span;
    std::string message;
    std::string expected;          // 可为空
    std::string actual;            // 可为空
    std::string related;           // 对象/参数/轨道
    std::vector<std::string> suggestions;
};
```

- 人类可读：`file:line:col: error[code]: message`，附 expected/actual/help。
- 机器可读：`--json` 输出包含文件、行列、offset、code、severity、expected、actual、related、suggestions 的数组。
- 诊断不依赖源码行号作为身份；行号只用于展示。

### 错误恢复

- Lexer 始终消费完整输入；非法字符/未闭合字符串产生诊断后继续。
- Parser 在声明/语句边界同步：`;`、`}`、声明关键字。
- 每个错误至少推进一个 token，避免死循环。
- 类型检查只在名称解析后运行一次；语法错误不会触发级联 lowering 错误。

## 4. Typed HIR

轻量 HIR 不复制完整 AST，而是保存：

```cpp
struct HirModule {
    const Ast* ast;
    std::vector<Type> exprTypes;       // 与 AST expr id 对齐
    std::vector<SymbolId> exprSymbols; // 标识符解析结果
    std::vector<Constant> exprConstants; // 标量/时长/音程常量
    std::vector<ResolvedDecl> declarations;
};
```

- 每个表达式有唯一 `Type`。
- 标识符在 Resolver 阶段绑定，TypeChecker 只使用 SymbolId。
- Lowering 只读取 HIR；不会重新做名称解析或类型推断。
- 这满足“类型检查通过后再构造 Core 对象”，同时避免第二套完整 AST。

## 5. 类型与单位

- 类型见 `docs/type-system.md`。
- 裸数字在期望位置转换为 Duration/Position/ScaleDegree/Scalar；显式单位不退化。
- `Duration` 以全音符有理数为内部单位；`bars`/`beats` 使用 project meter 精确换算。
- 时间域由 Curve 携带并在自动化/表情处检查。

## 6. ParameterRegistry

前端注册表提供：

- 参数名 → 单位、默认范围、允许作用域、允许模式；
- automation 目标与表达式通道校验；
- 值域检查（关键帧/端点）；
- 冲突检测（同目标同域多个 replace）。

注册表是语言/生产策略，不进入 Music IR。

## 7. Lowering

顺序：

1. 校验 project 存在且为第一个声明；
2. 创建 `Project`，设置 tempo/meter/key；
3. 按声明顺序求值 let/curve；
4. 创建全部 track（允许暂时空 clip）；
5. 构建 section 的 clip 定义（Fragment id、`at` 偏移、表情曲线 id、时长）；
6. 按 arrangement 顺序计算 section 起点并添加 clip；
7. 添加 automation；
8. 校验 clip 时长不超过 section；
9. 返回 Project + MIDI 导出名 + 诊断。

### 稳定实例身份

- 实例 key：`track:<track>|section:<section>|play:<n>|clip:<clip>`。
- `n` 是同一 section 在 arrangement 中的出现序号（0 起）。
- key 经 FNV-1a 映射为 32-bit `InstanceId`，写入 `ClipSpec.instanceId`。
- 冲突检测失败报 `sema-instance-id-collision`。
- 影响：
  - 重命名 track/section/clip 改变身份；
  - 在 arrangement 中插入同 section 的更早一次播放，会改变后续同 section 出现序号；
  - 插入其他 section、编辑音符、修改曲线不影响已有实例 id；
  - 源码行号不参与身份。
- 参数 id 仍是名称型，不受 lowering 顺序影响。

## 8. CLI

```text
mplc check <file> [--json]
mplc build <file> --output <dir> [--ppq N] [--json]
```

- `check`：Lexer/Parser/Resolver/Sema/Lowering 全流程，但不写出产物。
- `build`：成功后写 `music.ir`、`temporal.ir`、`production.ir`、`trace.json`、MIDI，并合并后端诊断。
- 诊断默认人类可读；`--json` 输出机器可读数组。

## 9. 测试

- Lexer：关键字、单位、rational、注释、字符串、非法字符。
- Parser：AST 结构、AST 黄金、错误恢复。
- Resolver：重复声明、未知名称、跨类别命名空间。
- Type：类型/单位/参数数量/时间域/自动化冲突。
- Lowering：section 越界、稳定实例 id、参数 id。
- 等价性：`.mpl` 与等价 C++ Core 构造的 MIDI 字节比较；Music/Temporal IR 做语义摘要比较。
- 黄金：Music IR / Temporal IR / Production IR / Trace（无计时）/ MIDI hex。
- 诊断：JSON 字段与人类格式。

## 10. 范围限制

不实现：用户函数、模块、生成算子、约束表达式、LLM、情绪/审美策略、实时 DSP、物理/采样域曲线求值。保留语法扩展点，但不加入无语义功能。
