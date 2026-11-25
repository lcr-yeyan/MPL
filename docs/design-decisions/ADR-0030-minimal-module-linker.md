# ADR-0030：最小模块链接器

状态：已采纳（V0.3.0-rc）

## 背景

现有前端是单文件全局 SymbolTable。需要最小模块/命名空间/导入/公开符号能力，但不引入
包管理器、动态模块或完整命名空间语法。

## 决策

1. 文件级模块：
   - 可选 `module Name;`；
   - `import "path";` 必须在 project 前；
   - `export fn/let/curve` 控制公开符号。
2. Frontend 链接阶段：
   - 递归加载 imports，规范化路径缓存；
   - DFS 检测循环；
   - 后序合并依赖；
   - 私有符号 mangle `__m<index>_<name>`；
   - 导出符号保留原名；冲突诊断；
   - 作用域感知改写函数体/let/curve 中的标识符与调用名；
   - ExprId 平移后追加到根 Module。
3. 模块内容身份：
   - FNV-1a 源文件哈希；
   - imports 规范化路径；
   - exports 原始名；
   - 写入 `ModuleRecord`、`Compilation::modules` 与 Trace。
4. 标准库是普通 `.mpl` 文件，通过 `std/` 搜索路径导入。
5. 库文件只允许函数/let/curve；其他声明诊断。

## 后果

- 不改变 Sema/Lowering 的核心模型；模块在链接后消失。
- 无前缀调用导出符号，冲突显式诊断。
- 不支持版本约束、别名、重导出、动态加载；这些留给后续版本。
