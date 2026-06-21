// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ChooserToolset.generated.h"

/// A Chooser table asset or nested chooser subobject found in the project.
USTRUCT(BlueprintType)
struct FChooserToolsetChooserRef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString NestedChooserName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString OutputObjectType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ResultType;
};

/// A reflected Chooser context parameter.
USTRUCT(BlueprintType)
struct FChooserToolsetContextInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ValueJson;
};

/// Reflected property binding information used by a Chooser column input.
USTRUCT(BlueprintType)
struct FChooserToolsetBindingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 ContextIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bIsBoundToRoot = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> PropertyPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString EnumType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString StructType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString AllowedClass;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString BindingJson;
};

/// A reflected Chooser column and its per-row values.
USTRUCT(BlueprintType)
struct FChooserToolsetColumnInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bDisabled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString InputType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString RowValuesProperty;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString InputValueType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString InputValueJson;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FChooserToolsetBindingInfo Binding;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> RowValuesJson;
};

/// One row cell joined with its source column metadata.
USTRUCT(BlueprintType)
struct FChooserToolsetCellInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 ColumnIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ColumnType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString InputType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString BindingDisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> BindingPropertyPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ValueJson;

	// @KUROGAMES BEGIN 行筛选条件可读化：该 cell 翻译后的可读筛选条件（如 "在地面 == 真"）；输出列等非筛选列为空
	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ConditionText;
	// @KUROGAMES END
};

/// A reflected Chooser row result.
USTRUCT(BlueprintType)
struct FChooserToolsetRowInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bDisabled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FChooserToolsetCellInfo> Cells;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ResultType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ResultJson;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ReferencedObject;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString NestedChooserPath;

	// @KUROGAMES BEGIN 行筛选条件可读化：本行所有有效筛选条件用 " 且 " 连接而成的一句话，便于一眼读懂
	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ConditionSummary;
	// @KUROGAMES END
};

/// Complete description of a Chooser table or nested chooser.
USTRUCT(BlueprintType)
struct FChooserToolsetDescription
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString NestedChooserName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString OutputObjectType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ResultType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> NestedChooserNames;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FChooserToolsetContextInfo> Context;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FChooserToolsetColumnInfo> Columns;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FChooserToolsetRowInfo> Rows;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString FallbackResultType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString FallbackResultJson;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString FallbackReferencedObject;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString FallbackNestedChooserPath;
};

/// One Chooser reached while recursively expanding NestedChooser references.
USTRUCT(BlueprintType)
struct FChooserToolsetNestedChooserNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 ParentIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 Depth = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 SourceRowIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString SourceKind;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString SourceResultType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ChooserPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bCycle = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<int32> ChildIndices;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FChooserToolsetDescription Description;
};

/// Flat representation of recursively expanded NestedChooser references.
USTRUCT(BlueprintType)
struct FChooserToolsetNestedChooserDescription
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString NestedChooserName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FChooserToolsetNestedChooserNode> Nodes;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> Errors;
};

/// Compact row information for large chooser tables.
USTRUCT(BlueprintType)
struct FChooserToolsetRowSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bDisabled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ResultType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ReferencedObject;
};

/// Compact column information for large chooser tables.
USTRUCT(BlueprintType)
struct FChooserToolsetColumnSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bDisabled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString InputType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString RowValuesProperty;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> SampleRowValuesJson;
};

/// Compact description of a Chooser table. Use this before DescribeChooser on large nested tables.
USTRUCT(BlueprintType)
struct FChooserToolsetSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString NestedChooserName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ResolvedChooserPath;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString OutputObjectType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString ResultType;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 RowCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	int32 ColumnCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> ContextTypes;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> DirectNestedChooserNames;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FChooserToolsetColumnSummary> Columns;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FChooserToolsetRowSummary> Rows;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString FallbackResultType;
};

/// A Chooser column/result struct type that can be instantiated by editor tools.
USTRUCT(BlueprintType)
struct FChooserToolsetStructTypeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString Tooltip;
};

/// Result of compiling and validating a chooser.
USTRUCT(BlueprintType)
struct FChooserToolsetCompileResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bCompiled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bValid = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> Messages;
};

/// Result of evaluating a chooser with supplied context values.
USTRUCT(BlueprintType)
struct FChooserToolsetEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	FString Status;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	bool bHadOutputs = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> SelectedObjects;

	UPROPERTY(BlueprintReadWrite, Category = "Chooser")
	TArray<FString> Messages;
};

/// A ProxyTable asset or ProxyAsset found in the project.
USTRUCT(BlueprintType)
struct FChooserToolsetProxyRef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString OutputObjectType;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString ResultType;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString Guid;
};

/// One ProxyTable entry after reflection.
USTRUCT(BlueprintType)
struct FChooserToolsetProxyTableEntryInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString SourceTable;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString ProxyAsset;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString Guid;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString LegacyKey;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString ValueType;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString ReferencedObject;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString ValueJson;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	int32 OutputStructCount = 0;
};

/// Description of a ProxyAsset.
USTRUCT(BlueprintType)
struct FChooserToolsetProxyAssetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString Guid;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString OutputObjectType;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString ResultType;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	TArray<FChooserToolsetContextInfo> Context;
};

/// Description of a ProxyTable asset.
USTRUCT(BlueprintType)
struct FChooserToolsetProxyTableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	TArray<FString> InheritedTables;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	TArray<FChooserToolsetProxyTableEntryInfo> EditorEntries;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	TArray<FChooserToolsetProxyTableEntryInfo> RuntimeEntries;

	UPROPERTY(BlueprintReadWrite, Category = "ProxyTable")
	TArray<FString> DuplicateGuids;
};

/// Provides tools for inspecting and editing Chooser tables.
UCLASS(BlueprintType, Hidden)
class UChooserToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns Chooser table assets in the project.
	 * @param OutputObjectType If non-empty, only returns choosers whose output object type path or name matches this string.
	 * @return Matching root chooser assets. Nested choosers are listed by DescribeChooser.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "Chooser")
	static TArray<FChooserToolsetChooserRef> ListChoosers(const FString& OutputObjectType);

	/**
	 * Describes a Chooser table, including context data, columns, row cell values, results, fallback, and nested chooser names.
	 * @param AssetPath Package/object path of the root UChooserTable asset.
	 * @param NestedChooserName Optional nested chooser subobject name. Leave empty for the root table.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "Chooser")
	static FChooserToolsetDescription DescribeChooser(const FString& AssetPath, const FString& NestedChooserName);

	/**
	 * Recursively describes a Chooser and every NestedChooser reachable from row results or fallback until leaf nodes.
	 * The result is a flat graph-like expansion: each node has an Index, ParentIndex, ChildIndices, source row, and full Chooser description.
	 * @param AssetPath Package/object path of the root UChooserTable asset.
	 * @param NestedChooserName Optional nested chooser subobject name. Leave empty for the root table.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "Chooser")
	static FChooserToolsetNestedChooserDescription DescribeNestedChoosers(const FString& AssetPath, const FString& NestedChooserName);

	/**
	 * Returns a compact Chooser description without serializing every cell and result struct.
	 * Use this for large nested chooser trees, then call DescribeChooser only for the exact row/table that needs full detail.
	 * @param AssetPath Root asset path or full subobject path, e.g. /Game/X.CHT or /Game/X.CHT:Parent.Child.
	 * @param NestedChooserName Optional nested chooser name/path. Supports direct subobject names and dotted paths such as "Stand Walks.Stand Walks F".
	 * @param MaxRows Maximum rows to include in the summary. Pass 0 to include all rows.
	 * @param MaxCellSamples Maximum sample cell values per column. Pass 0 to omit sample cell values.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "Chooser")
	static FChooserToolsetSummary DescribeChooserSummary(const FString& AssetPath, const FString& NestedChooserName, int32 MaxRows, int32 MaxCellSamples);

	/**
	 * Lists available Chooser column struct types that can be passed to AddColumn.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static TArray<FChooserToolsetStructTypeInfo> ListAvailableColumnTypes();

	/**
	 * Lists available Chooser result struct types that can be passed to SetRowResult or SetFallback.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static TArray<FChooserToolsetStructTypeInfo> ListAvailableResultTypes();

	/**
	 * Lists reflected properties on a UObject class or UScriptStruct for choosing binding paths.
	 * @param TypePath Class or struct path, for example /Script/Engine.Actor or /Script/Chooser.ChooserEvaluationInputObject.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static TArray<FString> ListContextProperties(const FString& TypePath);

	/**
	 * Creates a new Chooser table asset.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param PackagePath Package path for the asset, for example /Game/AI/CHT_Movement.
	 * @param ResultType ObjectResult, ClassResult, or NoPrimaryResult.
	 * @param OutputObjectType Optional class path for object/class results, for example /Script/Engine.AnimationAsset.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static FString CreateChooser(const FString& PackagePath, const FString& ResultType, const FString& OutputObjectType);

	/**
	 * Replaces root chooser context data from JSON.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param AssetPath Package/object path of the root UChooserTable asset.
	 * @param ContextDataJson JSON array of objects: [{"Type":"/Script/Chooser.ChooserEvaluationInputObject","Value":{...}}].
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void SetContextData(const FString& AssetPath, const FString& ContextDataJson);

	/**
	 * Adds a column to a Chooser table.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param ColumnType Struct path or name, for example /Script/Chooser.FloatRangeColumn or FFloatRangeColumn.
	 * @param InputType Optional input binding struct path or name; when empty, the column default is used.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static int32 AddColumn(const FString& AssetPath, const FString& NestedChooserName, const FString& ColumnType, const FString& InputType);

	/**
	 * Removes a column from a Chooser table.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void RemoveColumn(const FString& AssetPath, const FString& NestedChooserName, int32 ColumnIndex);

	/**
	 * Adds a row to a Chooser table.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @return The new row index.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static int32 AddRow(const FString& AssetPath, const FString& NestedChooserName);

	/**
	 * Removes a row from a Chooser table.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void RemoveRow(const FString& AssetPath, const FString& NestedChooserName, int32 RowIndex);

	/**
	 * Moves a row in a Chooser table. Re-describe the chooser afterward because row indices change.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void MoveRow(const FString& AssetPath, const FString& NestedChooserName, int32 SourceRowIndex, int32 TargetRowIndex);

	/**
	 * Sets a single cell value from JSON using the column's RowValuesPropertyName.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param CellJson JSON value or object matching the row value element type.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void SetCell(const FString& AssetPath, const FString& NestedChooserName, int32 ColumnIndex, int32 RowIndex, const FString& CellJson);

	/**
	 * Sets a row result from JSON.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param ResultType Struct path or name, for example /Script/Chooser.AssetChooser or FAssetChooser.
	 * @param ResultJson JSON object matching the result struct fields.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void SetRowResult(const FString& AssetPath, const FString& NestedChooserName, int32 RowIndex, const FString& ResultType, const FString& ResultJson);

	/**
	 * Sets the fallback result from JSON.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void SetFallback(const FString& AssetPath, const FString& NestedChooserName, const FString& ResultType, const FString& ResultJson);

	/**
	 * Enables or disables a Chooser row.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 */
	UFUNCTION(meta = (AICallable), Category = "Chooser")
	static void SetRowDisabled(const FString& AssetPath, const FString& NestedChooserName, int32 RowIndex, bool bDisabled);

	/**
	 * Compiles the chooser and runs UObject data validation.
	 * @param NestedChooserName Optional nested chooser subobject name. Leave empty for the root table.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "Chooser")
	static FChooserToolsetCompileResult CompileChooser(const FString& AssetPath, const FString& NestedChooserName);

	/**
	 * Evaluates a chooser with JSON-provided context values.
	 * @param ContextJson JSON array. Object entries use {"ObjectPath":"/Game/..."}; struct entries use {"Type":"/Script/Module.Struct","Value":{...}}.
	 * @return Selected object paths and the Chooser iterator status.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "Chooser")
	static FChooserToolsetEvaluationResult TestEvaluate(const FString& AssetPath, const FString& NestedChooserName, const FString& ContextJson);

	/**
	 * Returns ProxyTable assets in the project.
	 * @param OutputObjectType If non-empty, only returns tables containing at least one proxy whose output object type path or name matches this string.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "ProxyTable")
	static TArray<FChooserToolsetProxyRef> ListProxyTables(const FString& OutputObjectType);

	/**
	 * Returns ProxyAsset assets in the project.
	 * @param OutputObjectType If non-empty, only returns proxy assets whose output object type path or name matches this string.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "ProxyTable")
	static TArray<FChooserToolsetProxyRef> ListProxyAssets(const FString& OutputObjectType);

	/**
	 * Describes a ProxyAsset, including GUID, result type, output type, and context parameters.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "ProxyTable")
	static FChooserToolsetProxyAssetInfo DescribeProxyAsset(const FString& AssetPath);

	/**
	 * Describes a ProxyTable, including editor entries, inherited tables, runtime entries, and duplicate GUIDs.
	 * @param bIncludeValueJson Include full result JSON for each entry. Pass false for faster summaries.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "ProxyTable")
	static FChooserToolsetProxyTableInfo DescribeProxyTable(const FString& AssetPath, bool bIncludeValueJson);

	/**
	 * Finds ProxyTable entries that map the specified ProxyAsset.
	 * @param ProxyAssetPath The ProxyAsset object path to search for.
	 * @param ProxyTablePath Optional table path to restrict the search. Leave empty to search all ProxyTables.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "ProxyTable")
	static TArray<FChooserToolsetProxyTableEntryInfo> FindProxyMappings(const FString& ProxyAssetPath, const FString& ProxyTablePath);
};
