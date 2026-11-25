# MPL 最小模块规范（V0.3.0 Final）

状态：实现规范。对应代码：`mpl/frontend/Frontend.hpp`、`mpl/frontend/Frontend.cpp`、
`mpl/core/ModuleInfo.hpp`、`stdlib/*.mpl`。

## 1. 定位

模块系统解决单文件复用问题，不实现包管理器、动态模块、任意外部 I/O 或泛型。模块是
**编译期源码单元**：导入在 Frontend 链接阶段完成，Lowering/运行时不感知模块存在。

## 2. 语法

```text
module Library;

import "std/rhythm.mpl";

export fn helper(...) -> Fragment { ... }
fn privateHelper(...) -> Fragment { ... }
export let Shared = ...;
```

- `module Name;` 可选，必须是文件第一条语句；缺省用文件名 stem。
- `import "path";` 必须出现在 project 之前；库文件可只有 imports/声明。
- `export` 可修饰 `fn`、`let`、`curve`。
- 库文件不得包含 project/asset/track/section/automation/processor/bus/master/
  arrangement/export；违反报 `module-unsupported-declaration`。

## 3. 可见性

- 导出名进入导入者的全局命名空间，可无前缀调用；
- 私有名被链接器改写为 `__m<moduleIndex>_<name>`，模块内部调用同步改写；
- 不同模块的同名私有符号互不冲突；
- 两个模块导出同名符号报 `module-export-conflict`；
- 导出名与项目已有声明冲突同样报错。

没有 `module.name` 限定调用；导入即公开接口。这是 RC 的最小语义，不是完整命名空间系统。

## 4. 导入解析

搜索顺序：

1. 绝对路径；
2. 导入文件所在目录的相对路径；
3. `std/<file>` 映射到标准库目录；
4. `FrontendOptions::importPaths`；
5. 当前工作目录。

标准库目录由编译期 `MPL_STDLIB_DIR` 提供；测试与 CLI 使用同一默认搜索路径。

失败诊断：

| 代码 | 条件 |
| --- | --- |
| `module-import-missing` | 文件不存在/不可读/无法解析 |
| `module-import-cycle` | 导入图存在环 |
| `module-import-disabled` | options 禁止文件导入 |
| `module-unsupported-declaration` | 库文件含非函数/let/curve 声明 |
| `module-export-conflict` | 导出名冲突 |

## 5. 链接过程

1. 解析根文件；
2. 深度优先加载 imports（缓存规范化路径）；
3. 检测环；
4. 后序合并：依赖先合并；
5. 每个库：收集声明名 → 分配公开/私有名字 → 改写函数体/let/curve 中的标识符与调用名
   （尊重参数与局部 let/for 变量）→ 平移 ExprId → 追加声明/表达式；
6. 根 `declarationOrder` 保持原有索引；导入的 let/curve 追加为新的 DeclRef；
7. 记录模块身份。

## 6. 模块身份与 Trace

`ModuleRecord`：

```cpp
struct ModuleRecord {
    std::string name;
    std::string path;
    std::uint64_t contentHash;   // FNV-1a of source
    std::vector<std::string> imports;
    std::vector<std::string> exports;
};
```

- `contentHash` 对源文件字节计算；
- imports 为解析后的规范化路径；
- exports 为原始导出名；
- `Compilation::modules` 与 Trace 的 `modules` 输出；
- 传递依赖通过每个模块的 imports 图表达，不重复展开。

## 7. 标准库

RC 提供：

- `stdlib/rhythm.mpl`：pulseGrid / accentGrid / accentRhythm；
- `stdlib/harmony.mpl`：chordNotes / voicedBarAt / arpBarAt / bassBarAt / leadBarAt /
  arpBar / harmonyBarChord；
- `stdlib/voicing.mpl`：closeVoicing / nextVoicing / nextVoicingResult / sharedTones。

标准库是普通 MPL 模块，使用公开语言与内建；没有隐藏上下文、I/O 或编译器特权。

## 8. 限制

- 不支持目录模块、版本约束、别名、重导出；
- 不支持模块内 track/section/asset；
- 不支持动态加载；
- 模块内容哈希用于身份/溯源，不做增量编译缓存；
- 导出名冲突必须人工重命名（无自动命名空间前缀调用）。

## 9. 验收

`tests/test_modules.cpp` 覆盖：

- 导入/导出/私有符号；
- 导出冲突；
- 循环导入；
- 缺失导入；
- 不支持声明；
- 内容哈希与依赖记录；
- 传递导入；
- 模块函数度量上下文。
