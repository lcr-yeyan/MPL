# ADR-0009：前端采用“解析—绑定—类型—降低”四阶段

状态：已采纳（V0.1.0）

## 背景

如果 Parser 直接构造 Music IR，语法错误、未知名称和类型错误会污染 Core 状态，也难以提供一致的诊断与测试。

## 决策

1. Lexer 只产生 Token；Parser 只产生 AST。
2. Resolver 建立 SymbolTable，完成名称绑定与重复/未知名称检查。
3. TypeChecker 在绑定后的 AST 上做类型、单位、时间域、参数和自动化检查，产出轻量 Typed HIR。
4. 只有 Typed HIR 通过后，Lowering 才构造 `Project` / `MusicIR` / `TemporalIR`。
5. Parser 不做音乐求值、默认参数推导或 Core 对象构造。
6. 诊断统一为带 SourceSpan 的结构化对象，同时支持人类与 JSON 输出。

## 后果

- 非法程序不会产生半成品 IR。
- 语法错误可以被恢复并继续报告，类型错误不会级联到 lowering。
- 前端与 Core 的边界清晰，便于黄金测试与后续模块化。
- 代价是多一个 HIR 注解层，但它不复制完整 AST。
