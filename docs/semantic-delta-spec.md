# Semantic Delta Specification（V0）

状态：V0.4.0-beta

代码：`include/mpl/analysis/SemanticDelta.hpp`

## 1. 定位

Semantic Delta 比较两个已编译工程的**音乐语义**，而不是比较 ObjectId 或源文本。
ObjectId 在两次编译间可能变化；它本身不是音乐内容变化。

接口：

```cpp
SemanticDelta computeSemanticDelta(const Compilation& before,
                                   const Compilation& after);
```

## 2. 变化类别

`SemanticChange.change`：

- `added`
- `removed`
- `modified`

支持对象：

| objectType | key | 属性 |
| --- | --- | --- |
| `track` | track name | instrument / role / assetBindings |
| `clip` | track/clip#index | start / duration / symbolicEvents / instanceId / sourceObject |
| `section` | name#occurrence | start / duration |
| `harmony` | label#index | timeline（逻辑长度 + 事件） |
| `production` | project | tempo / meter / logicalExtent / mix / automation / temporalCurves |

## 3. 身份区分

每条变化带 `identity`：

| 值 | 含义 |
| --- | --- |
| `value` | 语义值身份：音高、时值、和声、生产参数 |
| `generation` | 生成来源身份：`sourceObject` 等 |
| `instance` | 播放实例身份：`instanceId` |

规则：

- ObjectId 变化不自动产生 value change；
- `instanceId` 变化只标记 instance；
- `sourceObject` 变化只标记 generation；
- 只有音高/时值/和声/参数变化才标记 value。

## 4. 原因分类

`cause`：

- `direct`：根模块 contentHash 改变；
- `dependency`：根模块不变，但传递依赖 resolvedHash 改变；
- `unknown`：无模块信息或无法归因。

当多个对象受同一共享依赖影响时，所有变化都标为 `dependency`；不把共享依赖的一次修改
误报为多个 independent direct edits。

## 5. Changed Observations

`changedObservations` 比较 Observation Registry 的 16 项观察：

- 值变化 → `changed`；
- 可用性变化 → `unverifiable`；
- 未变化不列出。

## 6. Unobserved

`unobserved` 明确列出比较范围外的属性。V0：

- `objectIds`

V0 已比较：轨道/Clip/Section、和声、符号事件、tempo/meter/logical extent、mix、automation
binding、temporal curves、asset bindings。

**未列出的属性不因不在 delta 中就被证明不变。** 依赖 Frame Contract 的覆盖范围。

## 7. JSON

```json
{
  "format": "mpl.semantic-delta",
  "version": 1,
  "causeSummary": "direct|dependency|unknown",
  "changes": [
    {
      "change": "modified",
      "objectType": "clip",
      "key": "Keys/Keys#0",
      "property": "symbolicEvents",
      "before": "[...]",
      "after": "[...]",
      "identity": "value",
      "cause": "direct",
      "source": "clip:Keys/Keys#0"
    }
  ],
  "changedObservations": [],
  "unobserved": ["objectIds"]
}
```

## 8. 限制

- 不做语义等价归一化（如等音拼写、声部重排）；
- 共享依赖归因是模块粒度，不是单个表达式粒度；
- 音频/渲染差异不在 delta 中，由 Render Manifest 处理；
- 不声称 delta 覆盖属性之外的不变。
