# ChooserToolset

`ChooserToolset` 是一个 Unreal Editor 插件，用于把 **Chooser / ProxyTable** 这类二进制配置资产导出成稳定、可读、可自动化分析的结构化数据。

它适合接入 CI、审计工具、Agent、批量检查脚本或自定义 Commandlet，避免用 T3D 文本、临时 Python 反射脚本或字符串解析去重复理解 Chooser 内部结构。

## 为什么需要它

Chooser 和 ProxyTable 通常承载“强逻辑配置”：行顺序、筛选条件、结果对象、fallback、NestedChooser、Proxy 映射都会影响运行时行为。直接 review `.uasset` 很困难，而普通文本导出又包含大量噪声。

`ChooserToolset` 的目标是提供一个 C++ 层的权威解析入口：

- 直接读取 `UChooserTable` / `UProxyTable` 的真实数据结构。
- 输出 column binding、row cells、result、fallback、NestedChooser graph 等语义字段。
- 让 Python、Commandlet、CI 或 Agent 只负责调用和落盘，不再自己写 parser。
- 为确定性 JSON diff、风险审计、历史快照对比提供稳定输入。

## 功能概览

- **Chooser 列表与描述**：列出项目中的 ChooserTable，描述 context、columns、rows、cells、results 和 fallback。
- **NestedChooser 递归展开**：从 root chooser 出发，把所有可达 `NestedChooser` 展成扁平 graph，保留 parent/source row/source result 信息。
- **ProxyTable 描述**：读取 ProxyTable editor/runtime entries、继承表、重复 GUID、ProxyAsset 映射和引用对象。
- **结构化 binding**：提取 `InputValue.Binding` 的 context index、property path、display name、enum/struct/class 类型。
- **行级 cells**：把每行 cell 和来源 column/binding 聚合好，调用方无需再手动 join `columns[].rowValues[rowIndex]`。
- **可读条件摘要**：提供 `conditionText` / `conditionSummary`，方便报告和人工 review。
- **只读校验与测试求值**：支持 compile/data validation 和 JSON context 的 test evaluate。
- **受控编辑 API**：提供创建 Chooser、增删行列、设置 cell/result/fallback 等 API；默认审计流程应只使用只读 API。

## 安装

把仓库放到项目或引擎插件目录，例如：

```text
<Project>/Plugins/ChooserToolset
```

或：

```text
<Engine>/Plugins/Experimental/Toolsets/ChooserToolset
```

插件依赖：

- Unreal Editor 目标。
- Unreal `Chooser` 插件。
- `ToolsetRegistry` 插件。

`ChooserToolset.uplugin` 已声明依赖，正常启用插件后会自动加载所需模块。

## 命令行启用

直接启用 `ChooserToolset`：

```bat
UnrealEditor-Cmd.exe Project.uproject -EnablePlugins=ChooserToolset -run=pythonscript -script="ExportChooser.py"
```

如果你的项目有聚合插件依赖 `ChooserToolset`，也可以启用聚合插件：

```bat
UnrealEditor-Cmd.exe Project.uproject -EnablePlugins=AllToolsets -run=pythonscript -script="ExportChooser.py"
```

## 快速开始：导出 NestedChooser

```python
import json
import unreal

asset_path = "/Game/Path/To/CHT_Example.CHT_Example"
out_path = r"D:\Temp\CHT_Example.nested.json"

nested = unreal.ChooserToolset.describe_nested_choosers(asset_path, "")

output = {
    "schemaVersion": 1,
    "source": "ChooserToolset.describe_nested_choosers",
    "asset": asset_path,
    "summary": {
        "nodeCount": len(nested.nodes),
        "errorCount": len(nested.errors),
        "maxDepth": max((node.depth for node in nested.nodes), default=0),
    },
    "nodes": [
        {
            "index": node.index,
            "parentIndex": node.parent_index,
            "depth": node.depth,
            "sourceRowIndex": node.source_row_index,
            "sourceKind": node.source_kind,
            "sourceResultType": node.source_result_type,
            "chooserPath": node.chooser_path,
            "cycle": node.cycle,
            "childIndices": list(node.child_indices),
            "rowCount": len(node.description.rows),
            "columnCount": len(node.description.columns),
        }
        for node in nested.nodes
    ],
    "errors": list(nested.errors),
}

with open(out_path, "w", encoding="utf-8") as handle:
    json.dump(output, handle, ensure_ascii=False, indent=2, sort_keys=True)
```

> Python 只建议负责调用和写文件。Chooser / ProxyTable 的语义解析应以 `ChooserToolset` C++ 输出为准。

## 常用 API

### Chooser

| API | 说明 |
|---|---|
| `list_choosers(output_object_type)` | 列出项目中的 ChooserTable。 |
| `describe_chooser(asset_path, nested_chooser_name)` | 描述 root 或指定 nested chooser。 |
| `describe_nested_choosers(asset_path, nested_chooser_name)` | 递归展开所有可达 `NestedChooser`。 |
| `describe_chooser_summary(asset_path, nested_chooser_name, max_rows, max_cell_samples)` | 大表摘要，避免导出全部 cell/result。 |
| `compile_chooser(asset_path, nested_chooser_name)` | 编译并执行数据校验。 |
| `test_evaluate(asset_path, nested_chooser_name, context_json)` | 使用 JSON context 测试求值。 |

### ProxyTable

| API | 说明 |
|---|---|
| `list_proxy_tables(output_object_type)` | 列出 ProxyTable。 |
| `list_proxy_assets(output_object_type)` | 列出 ProxyAsset。 |
| `describe_proxy_asset(asset_path)` | 描述 ProxyAsset 的 GUID、输出类型、result type 和 context。 |
| `describe_proxy_table(asset_path, include_value_json)` | 描述 ProxyTable entries、继承表和重复 GUID。 |
| `find_proxy_mappings(proxy_asset_path, proxy_table_path)` | 查找 ProxyAsset 映射。 |

> C++ 函数名是 PascalCase；Unreal Python 暴露后通常使用 snake_case。

## Chooser 输出重点字段

`describe_chooser(...)` 返回的 description 包含：

- `context`：Chooser context 声明和 raw JSON。
- `columns`：列类型、输入类型、row values、binding。
- `columns[].binding`：`contextIndex`、`displayName`、`propertyPath`、`enumType`、`structType`、`allowedClass` 等。
- `rows`：行禁用状态、cells、result、引用对象、NestedChooser 路径。
- `rows[].cells`：已按行聚合的 cell，包含来源 column 和 binding 信息。
- `rows[].conditionSummary`：本行可读条件摘要。
- `fallback`：fallback result、引用对象和 NestedChooser 路径。

`describe_nested_choosers(...)` 返回扁平 graph：

- `nodes[]`：root 和所有可达 nested chooser。
- `nodes[].parentIndex` / `childIndices`：拓扑关系。
- `nodes[].sourceRowIndex`：父 chooser 中触发该节点的 row。
- `nodes[].sourceKind`：`Root`、`RowResult` 或 `FallbackResult` 等来源。
- `nodes[].sourceResultType`：来源 result struct 类型。
- `nodes[].cycle`：是否检测到递归环。
- `errors[]`：加载或解析错误。

更完整的字段说明见 [`Docs/ChooserToolsetUsage.md`](Docs/ChooserToolsetUsage.md)。

## 推荐集成方式

### CI / Commandlet

推荐把 `ChooserToolset` 包装在项目自己的 Commandlet 或固定脚本里：

1. 扫描目标资产。
2. 调用 `describe_nested_choosers` / `describe_proxy_table`。
3. 写 UTF-8 JSON。
4. 对 JSON 做确定性归一化和 diff。
5. 把 diff 交给规则引擎、CI 或 Agent 分析。

Commandlet 可以负责批量调度、manifest、文件 IO、schema version、summary 和返回码；不要在 Commandlet 中重新实现 Chooser / ProxyTable 语义解析。

### Agent / LLM

Agent 应只消费 `ChooserToolset` 的结构化输出：

- 优先读 `binding`、`cells`、`referencedObject`、`nestedChooserPath`、`sourceResultType`。
- 不要从 `resultJson` 字符串里猜 nested target。
- 不要把 context wrapper type 当成业务输入类型。
- 不要在分析阶段临时启动 UE 补数据。

## 只读与写操作边界

默认审计和导出流程应使用只读 API。以下 API 会修改资产，调用前必须有明确授权，并由调用方负责版本管理、保存和回滚策略：

- `create_chooser`
- `set_context_data`
- `add_column` / `remove_column`
- `add_row` / `remove_row` / `move_row`
- `set_cell`
- `set_row_result`
- `set_fallback`
- `set_row_disabled`

## 仓库结构

```text
ChooserToolset/
  ChooserToolset.uplugin
  Source/ChooserToolset/
    ChooserToolset.Build.cs
    Private/ChooserToolset/
      ChooserToolset.h
      ChooserToolset.cpp
      Module.cpp
  Docs/
    ChooserToolsetUsage.md
```

`Binaries/`、`Intermediate/`、`Saved/` 等 Unreal 生成物已被 `.gitignore` 忽略。

## License

MIT License. See [`LICENSE`](LICENSE).
