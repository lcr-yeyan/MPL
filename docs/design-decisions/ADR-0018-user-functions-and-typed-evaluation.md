# ADR-0018：用户函数、作用域与类型化音乐求值

状态：已采纳（V0.3.0-alpha）

## 背景

V0.2.0 只有内建算子，无法复用创作算法。需要在不引入 VM/LLVM、不破坏 Music IR 依赖图的前提下，增加用户函数与最小控制流。

## 决策

1. 函数是纯函数：
   - 显式参数与返回类型；
   - 局部 `let` 不可变；
   - `return` 必须是最后一条语句；
   - 禁止修改 Project、执行 I/O、声明 Track/Section。
2. 作用域：
   - 全局 SymbolTable 不变；
   - 参数/局部变量使用 Sema/Lowering 的作用域栈，不写入全局表；
   - 函数名进入全局表用于重名检查。
3. 类型：
   - 新增 Integer、Boolean、Function；
   - 支持 Fragment、Curve、Duration、Position、Pitch、Int、Number、Scalar、Bool、String、Key、[T]；
   - 不允许隐式单位转换；数值允许 Int → Number 提升。
4. 求值：
   - 复用 Lowering 的 Value，函数返回 Music IR/Temporal IR 对象 id；
   - 不把结果扁平化为音符列表；
   - 不引入 LLVM 或虚拟机。
5. 控制流：
   - Bool、`!`、`&&`、`||`、比较；
   - `if` 表达式；
   - `for x in collection`；
   - `range`、`length`、`round`、`ratio`、`sequenceAll`、`parallelAll`；
   - 硬预算：集合 1024、循环 8192、调用深度 32、总调用 4096。
6. 禁止递归；调用图环路在 Sema 阶段拒绝。

## 后果

- 创作算法可以复用、组合并保留 Music IR 依赖。
- 函数与内建算子共享类型/单位检查。
- 递归、模块、函数值与跨编译缓存留待后续。
- 预算错误是硬诊断，不会静默截断。
