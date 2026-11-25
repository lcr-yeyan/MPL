# Collection Semantics（V0.5.0-beta）

## 1. 范围

本规范只定义**普通列表**的索引与切片。音乐时间/Duration/Rhythm 的切片是独立语义，
不通过本语法执行。

## 2. 索引

```text
P[0]
P[i]
```

- 索引类型：`Int`；其他类型诊断 `type-index`。
- 索引起点：0。
- 越界：`index-out-of-range`；不返回默认值。
- 空列表：`[]` 的任何索引都越界。
- 负索引：不支持，诊断 `index-negative`；不回绕。
- 非列表：诊断 `type-index-not-list`。
- 值语义：返回元素的独立值；列表不可变。
- 类型推导：`[T][Int] -> T`。
- 资源：O(1)。
- 求值：运行期检查边界；常量负索引在 Sema 阶段诊断。

## 3. 切片

```text
P[start:end]
P[:end]
P[start:]
P[:]
```

- 半开区间 `[start, end)`。
- start/end 均为 `Int`；缺省 start=0，缺省 end=length。
- start < 0 或 end < 0：`index-negative`。
- start > end：`index-slice-bounds`。
- start > length：`index-out-of-range`。
- end > length：**clamp 到 length**；不报错。
- 不提供 step/负步长；`P[::-1]` 不支持。
- 结果：新 `[T]`，值语义；`P[:]` 是副本。
- 资源：O(end-start)，结果大小受 1024 上限约束。
- 空切片 `P[1:1]` 合法，结果空列表。

## 4. 类型

- `[T]` 元素类型 `T` 可为 Pitch、Fragment、Int 等。
- 列表字面量必须同元素类型；空列表在无上下文时当前推导为 `[Number]`（已知限制）。
- 不引入嵌套泛型或索引赋值；列表不可变。

## 5. 诊断

| code | 条件 |
| --- | --- |
| `type-index-not-list` | 对非列表取索引 |
| `type-index` | 索引/切片边界不是 Int |
| `index-negative` | 负索引/负边界（常量或运行期） |
| `index-out-of-range` | 索引越界；切片 start 越过长度 |
| `index-slice-bounds` | start > end |
| `parse-expected-rbracket` | 缺 `]` |

## 6. 与音乐时间切片的区别

- 列表切片选择**值序列**；
- 音乐时间切片（Fragment/Rhythm span）必须保持 onset、duration、continuation、MeterMap 语义；
- 二者不共享语法/类型规则，不得把 `P[1:3]` 解释为“第 1 到 3 小节”。

## 7. 实现

- AST：`ExprKind::Index`，index 节点 `args={target,index}`；slice 节点 `text="slice"`，`args={target,start,end}`，end 可缺省。
- Sema：`checkIndex`。
- Lowering：边界检查、切片 clamp、值复制。
- 测试：`tests/test_collections.cpp`。

## 8. 已知限制

- 不支持负索引/步长；
- 空列表元素类型推导为 Number；
- 没有列表赋值/append/filter/map；
- 切片按值复制，尚无零拷贝优化。
