# 引擎 Chooser 求值语义（分析地基）

本文件是 **UE Chooser 运行时真实求值规则**的速查，供消费 `ChooserToolset` 输出、分析 Chooser 配置逻辑时据此判断"读得对不对"。它是把每列归类为 **筛选(filter) / 输出(output) / 评分(rank)** 的权威依据——分析 outline 的 `condition` / `inheritedCondition` 时必须以此为准。

> 引擎源码：`Engine/Plugins/Chooser/Source/Chooser`。求值主循环：`UChooserTable::EvaluateChooser`（`Private/Chooser.cpp`）。

## 求值主循环（怎么选出一行）

1. 初始装入**所有未禁用行**（cost 0）。
2. 每个 `HasFilters()==true` 的 column 依次执行 `Filter()`，对存活行集做 **AND 收窄**（无短路，每列都跑）。
3. 若有列贡献 cost（仅 `FloatDistanceColumn`），对存活行按 cost **升序排序**；否则保持表顺序。
4. 顺序遍历存活行，第一个产出结果的行胜出；选中后对**每个**非禁用 column 调用 `SetOutputs()`（输出列在此写回 context）。
5. 没有任何行产出 → 执行 `FallbackResult`，并用 fallback 特殊索引写各输出列的 `FallbackValue`。

关键推论：
- **没有筛选列的表 → 所有行按表顺序，首行胜出。**
- **条件完全相同的相邻行**（如 L/R 脚）→ 靠表顺序 + 输出列/下游区分，先出现者胜；这不是解析 bug。
- **未绑定/取不到值的输入 → passthrough（该列不施加约束）**，不是"全部淘汰"。
- `bDisabled` 的行/列：editor-only，cook 时物理裁剪；分析启用态即可。

## 各列语义（filter / output / rank）

| Column | 角色 | 行通过条件 / 说明 |
|---|---|---|
| **BoolColumn** | filter | `MatchAny` 恒过；否则 `输入 == 行存的 bool`。cell：MatchFalse/MatchTrue/MatchAny |
| **EnumColumn** | filter | `MatchEqual`→`==`；`MatchNotEqual`→`!=`；`MatchAny`→恒过；`Modulus` 运行时返回 false（淘汰） |
| **MultiEnumColumn** | filter | 行存位掩码 `Value`；`Value==0 \|\| (Value & (1<<输入))`（any-of）。`Value==0` 表示"任意" |
| **FloatRangeColumn** | filter | `(bNoMin\|\|输入>=Min) && (bNoMax\|\|输入<=Max)`，闭区间。两端都 No → 任意。wrap 模式(`bWrapInput` 且 `Max<Min`)变 OR(角度环) |
| **FloatDistanceColumn** | **rank(评分,非硬过滤)** | 默认不删行，按 `cost = CostMultiplier*min(\|输入-目标\|/MaxDistance,1)` 累加，最近者优先；**仅当 `bFilterOverMaxDistance=true`** 才额外硬过滤 `\|输入-目标\| < MaxDistance`。cell 是单个目标 float |
| **GameplayTagColumn** | filter | 受 `TagMatchType`(Any默认/All)、`TagMatchDirection`、`bMatchExact`、`bInvertMatchingLogic` 控制。空行容器在常规模式匹配一切 |
| **GameplayTagQueryColumn** | filter | `行查询.Matches(输入容器)`。**空查询匹配"无"**（与 GameplayTagColumn 空容器相反） |
| **ObjectColumn** | filter | `MatchEqual`/`MatchNotEqual`/`MatchAny`，按 SoftObjectPath 比较 |
| **ObjectClassColumn** | filter | `Equal`/`NotEqual`/`SubClassOf`(默认)/`NotSubClassOf`/`Any`。行类或输入为空→恒过 |
| **RandomizeColumn** | **不是条件** | 把存活集塌缩为 1 个加权随机行，必须放最后；也用于在等 cost 行间做随机平手 |
| **Output\*Column**（OutputBool/Enum/Float/Object/Struct/GameplayTagQuery） | **output(只写,不筛选)** | `HasFilters()==false`，从不收窄行；只在选中行上 `SetOutputs` 把 cell 写回 context。**绝不能当作筛选条件**——属于"效果/输出"，不该出现在 condition 里 |

## 给分析者的铁律

1. **"MatchAny / 空 / 未设" 在几乎所有列里是放宽，不是收紧**——唯一例外是 `GameplayTagQueryColumn`（空查询=匹配无）。
2. **Output\* 列永远不是条件**：它是选中后的赋值。`ChooserToolset` 的 `condition` 已正确排除它们。
3. **FloatDistance 默认是软评分**：把它当成硬区间约束会读错——它影响的是"在存活行中谁优先"，而非"谁存活"。
4. **某行实际生效条件 = node 的 `inheritedCondition` + 该行 `condition`**（outline 已把祖先已保证项上提）。
5. **行序即优先级**：分析可达性/遮蔽时，上方行先匹配会遮蔽下方同条件行。
6. fallback 仅在无行命中时生效。
