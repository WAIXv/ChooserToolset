# ChooserToolset 使用说明

`ChooserToolset` 是面向工具、自动化脚本和 Agent 的 Chooser / ProxyTable 结构化读写入口。它的目标是把二进制资产中有业务意义的结构导出为稳定、可遍历的数据，避免使用 T3D 文本、Python 反射脚本或临时 JSON parser 重复解释资产内部结构。

## 适用场景

- 批量检查 Chooser / ProxyTable 资产。
- 将 Chooser 资产导出为 Agent 或 CI 可分析的结构化描述。
- 递归展开 `NestedChooser`，查看从 root 到 leaf 的引用关系。
- 对比资产变更时生成确定性 JSON，再由外部 diff 程序比较。
- 在获得明确授权后，通过 Toolset API 修改 Chooser 行、列、cell、result 或 fallback。

## 加载插件

项目可直接启用 `ChooserToolset` 插件；如果项目已有聚合 Toolset 插件，也可以通过聚合插件依赖加载。headless 命令行通常形如：

```bat
UnrealEditor-Cmd.exe Project.uproject -EnablePlugins=ChooserToolset -run=pythonscript -script="ExportChooser.py"
```

如果聚合插件依赖了 `ChooserToolset`，也可以启用聚合插件：

```bat
UnrealEditor-Cmd.exe Project.uproject -EnablePlugins=AllToolsets -run=pythonscript -script="ExportChooser.py"
```

## 解析原则

`UChooserToolset` 是资产语义解析的权威入口：

- C++ 负责读取 `UChooserTable`、`UProxyTable`、column binding、row values、result 和 fallback。
- Python 或命令行脚本只负责加载项目、调用 Toolset、写文件。
- 外部 Agent / CI 只消费 Toolset 输出，不应再从 raw `ResultJson`、T3D 或 Python 反射结果里反推语义。
- 字段命名使用 `NestedChooser` / `Nested`，不要改成 `Tree`；嵌套引用可能是图状关系，不保证严格树结构。

## 常用只读 API

### Chooser

| API | 用途 |
|---|---|
| `ListChoosers(output_object_type)` | 列出项目中的 ChooserTable 资产，可按输出对象类型过滤。 |
| `DescribeChooser(asset_path, nested_chooser_name)` | 描述 root chooser 或指定 nested chooser 的完整结构。 |
| `DescribeNestedChoosers(asset_path, nested_chooser_name)` | 从 root 或指定 nested chooser 开始递归展开所有 `NestedChooser` 引用。 |
| `DescribeChooserSummary(asset_path, nested_chooser_name, max_rows, max_cell_samples)` | 大表快速摘要，避免一次序列化全部 cell/result。 |
| `CompileChooser(asset_path, nested_chooser_name)` | 编译并执行数据校验，返回 messages。 |
| `TestEvaluate(asset_path, nested_chooser_name, context_json)` | 使用 JSON context 测试求值，返回选择结果。 |

### ProxyTable

| API | 用途 |
|---|---|
| `ListProxyTables(output_object_type)` | 列出 ProxyTable。 |
| `ListProxyAssets(output_object_type)` | 列出 ProxyAsset。 |
| `DescribeProxyAsset(asset_path)` | 描述 ProxyAsset 的 GUID、输出类型、result type 和 context。 |
| `DescribeProxyTable(asset_path, include_value_json)` | 描述 ProxyTable 的 editor/runtime entries、继承表和重复 GUID。 |
| `FindProxyMappings(proxy_asset_path, proxy_table_path)` | 查找 ProxyAsset 在 ProxyTable 中的映射。 |

## Chooser 描述结构

`DescribeChooser` 返回 `FChooserToolsetDescription`：

| 字段 | 含义 |
|---|---|
| `assetPath` | root asset/object path。 |
| `nestedChooserName` | 当前描述的 nested chooser 名称；root 为空。 |
| `outputObjectType` | Chooser 输出对象类型。 |
| `resultType` | Chooser primary result 类型。 |
| `nestedChooserNames` | root chooser 上登记的 nested chooser 名称。 |
| `context[]` | context 声明，包含 wrapper type 和 raw JSON。 |
| `columns[]` | column 类型、输入类型、binding、row values。 |
| `rows[]` | row 禁用状态、condition summary、result、引用对象和 nested target。 |
| `fallback*` | fallback result、引用对象和 nested target。 |

### Context

`context[].type` 可能是包装结构，例如 context object / context struct wrapper。业务类型通常在 `context[].valueJson` 内部的 `struct`、`class`、`direction` 等字段里。使用者不要把 wrapper type 当成业务输入类型。

### Column Binding

`columns[].binding` 是从 column `InputValue.Binding` 反射得到的稳定结构：

| 字段 | 含义 |
|---|---|
| `contextIndex` | 绑定的 context 下标。 |
| `isBoundToRoot` | 是否绑定到 root。 |
| `displayName` | 编辑器显示名。 |
| `propertyPath[]` | 绑定属性链，适合做 diff key 和可读摘要。 |
| `enumType` | enum binding 的枚举类型。 |
| `structType` | struct binding 的结构类型。 |
| `allowedClass` | object binding 允许的 class。 |
| `bindingJson` | 原始 binding JSON，主要用于排障。 |

### Row Conditions

`rows[].conditionSummary` 已经由 C++ 按行把筛选 column 的 row values 翻译成可读条件。使用者不需要再手动 join `columns[].rowValuesJson[rowIndex]`，也不要在 Python 层重复解释 cell JSON。

`conditionSummary` 会把有效筛选条件拼成一句话；没有筛选条件时返回 `（无筛选条件，命中任意输入）`。LLM outline 接口会把这句进一步压缩为 `任意`。

### Result 与 NestedChooser

`rows[]` 提供结构化 result 信息：

| 字段 | 含义 |
|---|---|
| `resultType` | result struct 类型。 |
| `resultJson` | 原始 result JSON。 |
| `referencedObject` | result 引用的对象路径。 |
| `nestedChooserPath` | result 指向的 nested chooser 路径；非 nested result 为空。 |

使用者应优先读取 `referencedObject` / `nestedChooserPath`，不要从 `resultJson` 自行猜测。

## 递归 NestedChooser 描述

`DescribeNestedChoosers` 返回 `FChooserToolsetNestedChooserDescription`，用扁平节点表示嵌套图：

| 字段 | 含义 |
|---|---|
| `nodes[]` | root 和所有可达 nested chooser。 |
| `errors[]` | 加载、解析、递归错误。 |

每个 node 包含：

| 字段 | 含义 |
|---|---|
| `index` | 当前 node 下标。 |
| `parentIndex` | 父 node 下标；root 为 `-1`。 |
| `depth` | 递归深度。 |
| `sourceRowIndex` | 父 chooser 中触发该 nested chooser 的 row index；fallback 为 `-1`。 |
| `sourceKind` | 来源类型，例如 `Root`、`RowResult`、`FallbackResult`。 |
| `sourceResultType` | 来源 result struct 类型。 |
| `chooserPath` | 当前 chooser 的可加载路径或 subobject path。 |
| `cycle` | 是否检测到递归环。 |
| `childIndices[]` | 子 node 下标。 |
| `description` | 当前 chooser 的完整 `DescribeChooser` 结果。 |

外部导出层可以根据 `parentIndex` / `sourceRowIndex` / `sourceResultType` 生成 `edges[]`，但不应改变 node 内事实字段。

## LLM 层级概要

`DescribeNestedChooserOutline` 返回 `FChooserToolsetNestedChooserOutline`，是最终交给 Agent/LLM 的推荐入口。它复用 `DescribeNestedChoosers` 的 C++ 解析结果，只保留层级、每行条件和输出目标，避免把原始 column/cell/result JSON 暴露给 LLM。

顶层字段：

| 字段 | 含义 |
|---|---|
| `assetPath` | root Chooser 资产路径。 |
| `nestedChooserName` | 请求的起始 nested chooser；root 为空。 |
| `nodes[]` | root 和所有可达 nested chooser 的精简节点。 |
| `errors[]` | 加载、解析、递归错误。 |

每个 node 包含：

| 字段 | 含义 |
|---|---|
| `index` | 当前 node 下标。 |
| `parentIndex` | 父 node 下标；root 为 `-1`。 |
| `depth` | 递归深度。 |
| `sourceRowIndex` | 父 chooser 中触发该 nested chooser 的 row index；root 为 `-1`。 |
| `name` | 压缩后的 Chooser 名称，便于 LLM 阅读。 |
| `chooserPath` | 当前 chooser 的完整路径。 |
| `cycle` | 是否检测到递归环。 |
| `childIndices[]` | 子 node 下标。 |
| `rows[]` | 当前 chooser 的行条件和目标。 |

每个 row 包含：

| 字段 | 含义 |
|---|---|
| `index` | 行号，保留 Chooser 选择顺序。 |
| `disabled` | 行是否禁用。 |
| `condition` | C++ 已翻译的行筛选条件；无筛选条件压缩为 `任意`。 |
| `targetKind` | `NestedChooser`、`Object` 或 `None`。 |
| `target` | 压缩后的目标名。 |
| `targetNodeIndex` | `targetKind=NestedChooser` 时对应的 node 下标，否则为 `-1`。 |

Python/Commandlet 可以把这个结构直接写成 UTF-8 JSON，或按 `parentIndex` / `childIndices` 排版成 Markdown 树；不要在 Python 里重新解释 `rowValuesJson`、`resultJson` 或 Chooser 内部 struct。

## ProxyTable 描述结构

`DescribeProxyTable(asset_path, include_value_json)` 返回：

| 字段 | 含义 |
|---|---|
| `assetPath` | ProxyTable 路径。 |
| `inheritedTables[]` | 继承或引用的表。 |
| `editorEntries[]` | 编辑器条目。 |
| `runtimeEntries[]` | runtime 条目。 |
| `duplicateGuids[]` | 重复 GUID。 |

Entry 包含 `proxyAsset`、`guid`、`legacyKey`、`valueType`、`referencedObject`、`valueJson`、`outputStructCount`。当只需要快速扫描时，可传 `include_value_json=false`。

## 修改 API 安全约定

以下 API 会修改资产，必须在调用前获得明确授权，并由调用方负责版本管理和保存策略：

- `CreateChooser`
- `SetContextData`
- `AddColumn` / `RemoveColumn`
- `AddRow` / `RemoveRow` / `MoveRow`
- `SetCell`
- `SetRowResult`
- `SetFallback`
- `SetRowDisabled`

默认审计、导出、diff 流程应只使用只读 API。

## 建议的导出规范

外部导出脚本可以在 Toolset 结果外层补充：

- `schemaVersion`
- `source`
- `toolset`
- `generatedAt`
- `summary`
- `edges[]`

用于 diff 的归一化程序建议忽略 `generatedAt`，对对象 key 做稳定排序，并优先比较结构化字段：`context`、`columns.binding`、`rows.conditionSummary`、`rows.resultType`、`rows.referencedObject`、`rows.nestedChooserPath`、`fallback`、`nodes` 和 `errors`。

## 反模式

- 使用 T3D 文本 diff 作为语义 diff 输入。
- 使用 Python 反射遍历 `UChooserTable` 并自行解释 `Binding`、`RowValues`、`Result`。
- 在 Agent 分析阶段临时启动 UE 补数据。
- 根据 `resultJson` 字符串自行判断 nested target，而不是读取 `nestedChooserPath`。
- 把 context wrapper type 当成业务输入类型。
