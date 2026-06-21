// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserToolset/ChooserToolset.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chooser.h"
#include "Dom/JsonObject.h"
#include "IChooserColumn.h"
#include "IHasContext.h"
#include "IObjectChooser.h"
#include "JsonObjectConverter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/DataValidation.h"
#include "ObjectChooser_Asset.h"
#include "ObjectChooser_Class.h"
#include "ProxyAsset.h"
#include "ProxyTable.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/Script.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChooserToolset)

namespace
{
	FString PropertyValueToJson(FProperty* Property, const void* Value);
	FString StructMemoryToJson(const UScriptStruct* StructType, const void* StructMemory);
	FString GetReferencedObjectPath(const FInstancedStruct& Struct);
	bool TryExtractNestedChooserPathFromJson(const FString& ResultJson, FString& OutChooserPath);

	void RaiseChooserToolsetError(const FString& Message)
	{
		UKismetSystemLibrary::RaiseScriptError(Message);
	}

	FString StructTypeName(const FInstancedStruct& Struct)
	{
		return Struct.GetScriptStruct() ? Struct.GetScriptStruct()->GetPathName() : FString();
	}

	FString StructToJson(const FInstancedStruct& Struct)
	{
		if (!Struct.IsValid())
		{
			return TEXT("{}");
		}

		FString Json;
		FJsonObjectConverter::UStructToJsonObjectString(
			Struct.GetScriptStruct(), Struct.GetMemory(), Json, 0, 0, 0, nullptr, false);
		return Json;
	}

	TSharedPtr<FJsonValue> ParseJsonValue(const FString& Json)
	{
		TSharedPtr<FJsonValue> Value;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Value) || !Value.IsValid())
		{
			return nullptr;
		}
		return Value;
	}

	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			return nullptr;
		}
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> ParseJsonArray(const FString& Json)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Array))
		{
			Array.Reset();
		}
		return Array;
	}

	UScriptStruct* FindStructType(const FString& TypeName, const UScriptStruct* RequiredBase = nullptr)
	{
		if (TypeName.IsEmpty())
		{
			return nullptr;
		}

		TArray<FString> Candidates;
		Candidates.Add(TypeName);
		if (!TypeName.StartsWith(TEXT("/Script/")) && !TypeName.Contains(TEXT(".")))
		{
			const FString ShortName = TypeName.StartsWith(TEXT("F")) ? TypeName.RightChop(1) : TypeName;
			Candidates.Add(FString::Printf(TEXT("/Script/Chooser.%s"), *ShortName));
			Candidates.Add(FString::Printf(TEXT("/Script/Chooser.%s"), *TypeName));
		}

		for (const FString& Candidate : Candidates)
		{
			if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *Candidate))
			{
				if (!RequiredBase || Struct->IsChildOf(RequiredBase))
				{
					return Struct;
				}
			}
		}

		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			if (RequiredBase && !Struct->IsChildOf(RequiredBase))
			{
				continue;
			}
			if (Struct->GetName() == TypeName || Struct->GetName() == TypeName.RightChop(TypeName.StartsWith(TEXT("F")) ? 1 : 0))
			{
				return Struct;
			}
		}

		return nullptr;
	}

	UClass* FindClassType(const FString& TypeName)
	{
		if (TypeName.IsEmpty())
		{
			return nullptr;
		}

		if (UClass* Class = FindObject<UClass>(nullptr, *TypeName))
		{
			return Class;
		}

		return LoadObject<UClass>(nullptr, *TypeName);
	}

	UObject* LoadObjectFromPath(const FString& ObjectPath)
	{
		if (ObjectPath.IsEmpty())
		{
			return nullptr;
		}

		UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
		if (!Object)
		{
			Object = FSoftObjectPath(ObjectPath).TryLoad();
		}
		return Object;
	}

	FString StripChooserObjectPathSyntax(FString Path)
	{
		Path.TrimStartAndEndInline();
		Path.RemoveFromStart(TEXT("/Script/Chooser.ChooserTable'"));
		Path.RemoveFromEnd(TEXT("'"));
		return Path;
	}

	UChooserTable* LoadRootChooser(const FString& AssetPath)
	{
		UObject* Object = LoadObjectFromPath(AssetPath);
		UChooserTable* Chooser = Cast<UChooserTable>(Object);
		if (!Chooser)
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("Chooser asset '%s' could not be loaded."), *AssetPath));
		}
		return Chooser;
	}

	UChooserTable* TryLoadChooserNoError(const FString& AssetPath)
	{
		return Cast<UChooserTable>(LoadObjectFromPath(StripChooserObjectPathSyntax(AssetPath)));
	}

	UProxyTable* LoadProxyTable(const FString& AssetPath)
	{
		UProxyTable* ProxyTable = Cast<UProxyTable>(LoadObjectFromPath(AssetPath));
		if (!ProxyTable)
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("ProxyTable asset '%s' could not be loaded."), *AssetPath));
		}
		return ProxyTable;
	}

	UProxyAsset* LoadProxyAsset(const FString& AssetPath)
	{
		UProxyAsset* ProxyAsset = Cast<UProxyAsset>(LoadObjectFromPath(AssetPath));
		if (!ProxyAsset)
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("ProxyAsset '%s' could not be loaded."), *AssetPath));
		}
		return ProxyAsset;
	}

	UChooserTable* ResolveChooser(const FString& AssetPath, const FString& NestedChooserName)
	{
		const FString CleanAssetPath = StripChooserObjectPathSyntax(AssetPath);
		UChooserTable* RootChooser = TryLoadChooserNoError(CleanAssetPath);
		if (!RootChooser)
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("Chooser asset '%s' could not be loaded."), *AssetPath));
			return nullptr;
		}
		if (!RootChooser || NestedChooserName.IsEmpty())
		{
			return RootChooser;
		}

		const FString CleanNestedName = StripChooserObjectPathSyntax(NestedChooserName);
		if (UChooserTable* DirectNestedPath = TryLoadChooserNoError(CleanNestedName))
		{
			return DirectNestedPath;
		}
		if (UChooserTable* ColonNestedPath = TryLoadChooserNoError(CleanAssetPath + TEXT(":") + CleanNestedName))
		{
			return ColonNestedPath;
		}
		if (CleanAssetPath.Contains(TEXT(":")))
		{
			if (UChooserTable* DottedChildPath = TryLoadChooserNoError(CleanAssetPath + TEXT(".") + CleanNestedName))
			{
				return DottedChildPath;
			}
		}

		for (UChooserTable* NestedChooser : RootChooser->NestedChoosers)
		{
			if (NestedChooser && (NestedChooser->GetName() == CleanNestedName || NestedChooser->GetPathName().EndsWith(CleanNestedName)))
			{
				return NestedChooser;
			}
		}

		RaiseChooserToolsetError(FString::Printf(
			TEXT("Nested chooser '%s' was not found in '%s'."), *NestedChooserName, *AssetPath));
		return nullptr;
	}

	int32 GetRowCount(const UChooserTable* Chooser)
	{
		return Chooser ? Chooser->ResultsStructs.Num() : 0;
	}

	void MarkChooserChanged(UChooserTable* Chooser)
	{
		if (!Chooser)
		{
			return;
		}

		UChooserTable* RootChooser = Chooser->GetRootChooser();
		RootChooser->MarkPackageDirty();
		Chooser->MarkPackageDirty();
	}

	FChooserColumnBase* GetColumn(UChooserTable* Chooser, int32 ColumnIndex)
	{
		if (!Chooser || !Chooser->ColumnsStructs.IsValidIndex(ColumnIndex))
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("Column index %d is out of range."), ColumnIndex));
			return nullptr;
		}

		FChooserColumnBase* Column = Chooser->ColumnsStructs[ColumnIndex].GetMutablePtr<FChooserColumnBase>();
		if (!Column)
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("Column %d is not a Chooser column."), ColumnIndex));
		}
		return Column;
	}

	FString ResultTypeToString(EObjectChooserResultType ResultType)
	{
		switch (ResultType)
		{
		case EObjectChooserResultType::ObjectResult:
			return TEXT("ObjectResult");
		case EObjectChooserResultType::ClassResult:
			return TEXT("ClassResult");
		case EObjectChooserResultType::NoPrimaryResult:
			return TEXT("NoPrimaryResult");
		default:
			return TEXT("Unknown");
		}
	}

	bool ParseResultType(const FString& InResultType, EObjectChooserResultType& OutResultType)
	{
		if (InResultType.Equals(TEXT("ObjectResult"), ESearchCase::IgnoreCase) ||
			InResultType.Equals(TEXT("Object"), ESearchCase::IgnoreCase))
		{
			OutResultType = EObjectChooserResultType::ObjectResult;
			return true;
		}
		if (InResultType.Equals(TEXT("ClassResult"), ESearchCase::IgnoreCase) ||
			InResultType.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
		{
			OutResultType = EObjectChooserResultType::ClassResult;
			return true;
		}
		if (InResultType.Equals(TEXT("NoPrimaryResult"), ESearchCase::IgnoreCase) ||
			InResultType.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			OutResultType = EObjectChooserResultType::NoPrimaryResult;
			return true;
		}
		return false;
	}

	FChooserToolsetStructTypeInfo MakeStructTypeInfo(const UScriptStruct* Struct)
	{
		FChooserToolsetStructTypeInfo Info;
		if (!Struct)
		{
			return Info;
		}

		Info.Type = Struct->GetPathName();
		Info.DisplayName = Struct->GetDisplayNameText().ToString();
		Info.Category = Struct->GetMetaData(TEXT("Category"));
		Info.Tooltip = Struct->GetToolTipText().ToString();
		return Info;
	}

	TArray<FChooserToolsetStructTypeInfo> ListStructTypes(const UScriptStruct* BaseStruct)
	{
		TArray<FChooserToolsetStructTypeInfo> Result;
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			if (Struct != BaseStruct && Struct->IsChildOf(BaseStruct) && !Struct->HasMetaData(TEXT("Hidden")))
			{
				Result.Add(MakeStructTypeInfo(Struct));
			}
		}

		Result.Sort([](const FChooserToolsetStructTypeInfo& A, const FChooserToolsetStructTypeInfo& B)
		{
			return A.DisplayName < B.DisplayName;
		});
		return Result;
	}

	bool FillInstancedStructFromJson(FInstancedStruct& InstancedStruct, const FString& StructType, const FString& Json, const UScriptStruct* RequiredBase)
	{
		UScriptStruct* ScriptStruct = FindStructType(StructType, RequiredBase);
		if (!ScriptStruct)
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("Struct type '%s' was not found or has the wrong base type."), *StructType));
			return false;
		}

		InstancedStruct.InitializeAs(ScriptStruct);
		if (!Json.IsEmpty())
		{
			const TSharedPtr<FJsonObject> JsonObject = ParseJsonObject(Json);
			if (!JsonObject.IsValid())
			{
				RaiseChooserToolsetError(FString::Printf(TEXT("Expected JSON object for struct '%s'."), *StructType));
				return false;
			}

			FText FailReason;
			if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), ScriptStruct, InstancedStruct.GetMutableMemory(), 0, 0, false, &FailReason))
			{
				RaiseChooserToolsetError(FString::Printf(TEXT("Failed to convert JSON to '%s': %s"), *StructType, *FailReason.ToString()));
				return false;
			}
		}
		return true;
	}

	void FillBindingInfoFromStructMemory(const UScriptStruct* StructType, const void* StructMemory, FChooserToolsetBindingInfo& BindingInfo)
	{
		if (!StructType || !StructMemory)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->GetFName() != TEXT("Binding"))
			{
				continue;
			}

			const void* BindingMemory = Property->ContainerPtrToValuePtr<void>(StructMemory);
			BindingInfo.BindingJson = PropertyValueToJson(Property, BindingMemory);

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (FArrayProperty* ChainProperty = FindFProperty<FArrayProperty>(StructProperty->Struct, TEXT("PropertyBindingChain")))
				{
					FScriptArrayHelper ChainHelper(ChainProperty, ChainProperty->ContainerPtrToValuePtr<void>(BindingMemory));
					for (int32 Index = 0; Index < ChainHelper.Num(); ++Index)
					{
						if (const FNameProperty* NameProperty = CastField<FNameProperty>(ChainProperty->Inner))
						{
							BindingInfo.PropertyPath.Add(NameProperty->GetPropertyValue(ChainHelper.GetRawPtr(Index)).ToString());
						}
					}
				}

				if (FIntProperty* ContextIndexProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("ContextIndex")))
				{
					BindingInfo.ContextIndex = ContextIndexProperty->GetPropertyValue_InContainer(BindingMemory);
				}
				if (FBoolProperty* RootProperty = FindFProperty<FBoolProperty>(StructProperty->Struct, TEXT("IsBoundToRoot")))
				{
					BindingInfo.bIsBoundToRoot = RootProperty->GetPropertyValue_InContainer(BindingMemory);
				}
				if (FStrProperty* DisplayNameProperty = FindFProperty<FStrProperty>(StructProperty->Struct, TEXT("DisplayName")))
				{
					BindingInfo.DisplayName = DisplayNameProperty->GetPropertyValue_InContainer(BindingMemory);
				}
				if (FObjectProperty* EnumProperty = FindFProperty<FObjectProperty>(StructProperty->Struct, TEXT("Enum")))
				{
					if (UObject* EnumObject = EnumProperty->GetObjectPropertyValue_InContainer(BindingMemory))
					{
						BindingInfo.EnumType = EnumObject->GetPathName();
					}
				}
				if (FObjectProperty* StructTypeProperty = FindFProperty<FObjectProperty>(StructProperty->Struct, TEXT("StructType")))
				{
					if (UObject* StructObject = StructTypeProperty->GetObjectPropertyValue_InContainer(BindingMemory))
					{
						BindingInfo.StructType = StructObject->GetPathName();
					}
				}
				if (FObjectProperty* AllowedClassProperty = FindFProperty<FObjectProperty>(StructProperty->Struct, TEXT("AllowedClass")))
				{
					if (UObject* ClassObject = AllowedClassProperty->GetObjectPropertyValue_InContainer(BindingMemory))
					{
						BindingInfo.AllowedClass = ClassObject->GetPathName();
					}
				}
			}
			return;
		}
	}

	FChooserToolsetBindingInfo DescribeInputBinding(FChooserColumnBase* Column)
	{
		FChooserToolsetBindingInfo BindingInfo;
		if (!Column)
		{
			return BindingInfo;
		}

		FChooserParameterBase* InputValue = Column->GetInputValue();
		if (!InputValue)
		{
			return BindingInfo;
		}

		const UScriptStruct* InputStruct = Column->GetInputType();
		FillBindingInfoFromStructMemory(InputStruct, InputValue, BindingInfo);
		return BindingInfo;
	}

	FString GetInputValueJson(FChooserColumnBase* Column)
	{
		FChooserParameterBase* InputValue = Column ? Column->GetInputValue() : nullptr;
		const UScriptStruct* InputStruct = Column ? Column->GetInputType() : nullptr;
		return (InputValue && InputStruct) ? StructMemoryToJson(InputStruct, InputValue) : FString();
	}

	FChooserToolsetColumnInfo DescribeColumn(const FInstancedStruct& ColumnStruct, int32 ColumnIndex, int32 RowCount)
	{
		FChooserToolsetColumnInfo Info;
		Info.Index = ColumnIndex;
		Info.Type = StructTypeName(ColumnStruct);
		if (const UScriptStruct* ScriptStruct = ColumnStruct.GetScriptStruct())
		{
			Info.DisplayName = ScriptStruct->GetDisplayNameText().ToString();
		}

		FChooserColumnBase* Column = const_cast<FInstancedStruct&>(ColumnStruct).GetMutablePtr<FChooserColumnBase>();
		if (!Column)
		{
			return Info;
		}

		Info.bDisabled = Column->bDisabled;
		Info.InputType = Column->GetInputType() ? Column->GetInputType()->GetPathName() : FString();
		Info.InputValueType = Info.InputType;
		Info.InputValueJson = GetInputValueJson(Column);
		Info.Binding = DescribeInputBinding(Column);
		Info.RowValuesProperty = Column->RowValuesPropertyName().ToString();

		const UScriptStruct* ScriptStruct = ColumnStruct.GetScriptStruct();
		if (!ScriptStruct || Info.RowValuesProperty.IsEmpty())
		{
			return Info;
		}

		FProperty* RowValuesProperty = ScriptStruct->FindPropertyByName(Column->RowValuesPropertyName());
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(RowValuesProperty);
		if (!ArrayProperty)
		{
			return Info;
		}

		FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ColumnStruct.GetMemory()));
		const int32 ValueCount = FMath::Min(RowCount, Helper.Num());
		for (int32 RowIndex = 0; RowIndex < ValueCount; ++RowIndex)
		{
			TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(
				ArrayProperty->Inner, Helper.GetRawPtr(RowIndex), 0, 0);
			FString RowJson;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RowJson);
			FJsonSerializer::Serialize(JsonValue.ToSharedRef(), TEXT(""), Writer);
			Info.RowValuesJson.Add(RowJson);
		}
		return Info;
	}

	// @KUROGAMES BEGIN 行筛选条件可读化：把单个 cell 的列类型/绑定名/取值 JSON 翻译成人类可读的筛选条件
	struct FCellConditionResult
	{
		FString Text;                 // 可读条件文本；非筛选列为空字符串
		bool bIsFilterColumn = false; // 是否为筛选列（OutputStructColumn 等输出列为 false）
		bool bConstrains = false;     // 是否构成实际约束（"任意"/无界区间为 false，不计入行汇总）
	};

	FString StripEnumValueName(const FString& ValueName)
	{
		// 去掉 "EType::" 前缀，只保留枚举项短名，如 "ESpecialState::None" -> "None"
		int32 SeparatorIndex = INDEX_NONE;
		if (ValueName.FindLastChar(TEXT(':'), SeparatorIndex))
		{
			return ValueName.RightChop(SeparatorIndex + 1);
		}
		return ValueName;
	}

	FCellConditionResult BuildCellCondition(const FString& ColumnType, const FString& BindingDisplayName, const FString& ValueJson)
	{
		FCellConditionResult Result;
		const FString Label = BindingDisplayName.IsEmpty() ? TEXT("?") : BindingDisplayName;

		if (ColumnType.EndsWith(TEXT("EnumColumn")))
		{
			Result.bIsFilterColumn = true;
			const TSharedPtr<FJsonObject> ValueObject = ParseJsonObject(ValueJson);
			if (!ValueObject.IsValid())
			{
				return Result;
			}

			FString Comparison;
			ValueObject->TryGetStringField(TEXT("comparison"), Comparison);
			FString ValueName;
			if (ValueObject->TryGetStringField(TEXT("valueName"), ValueName) && !ValueName.IsEmpty())
			{
				ValueName = StripEnumValueName(ValueName);
			}
			else
			{
				// 非编辑器数据可能没有 valueName，回退到原始枚举数值
				double RawValue = 0.0;
				ValueObject->TryGetNumberField(TEXT("value"), RawValue);
				ValueName = FString::FromInt(static_cast<int32>(RawValue));
			}

			if (Comparison == TEXT("MatchEqual"))
			{
				Result.Text = FString::Printf(TEXT("%s == %s"), *Label, *ValueName);
				Result.bConstrains = true;
			}
			else if (Comparison == TEXT("MatchNotEqual"))
			{
				Result.Text = FString::Printf(TEXT("%s != %s"), *Label, *ValueName);
				Result.bConstrains = true;
			}
			else
			{
				// MatchAny 或其它：不构成约束
				Result.Text = FString::Printf(TEXT("%s: 任意"), *Label);
			}
			return Result;
		}

		if (ColumnType.EndsWith(TEXT("BoolColumn")))
		{
			Result.bIsFilterColumn = true;
			FString Comparison;
			if (const TSharedPtr<FJsonValue> ValueNode = ParseJsonValue(ValueJson))
			{
				Comparison = ValueNode->AsString();
			}

			if (Comparison == TEXT("MatchTrue"))
			{
				Result.Text = FString::Printf(TEXT("%s == 真"), *Label);
				Result.bConstrains = true;
			}
			else if (Comparison == TEXT("MatchFalse"))
			{
				Result.Text = FString::Printf(TEXT("%s == 假"), *Label);
				Result.bConstrains = true;
			}
			else
			{
				// MatchAny 或其它：不构成约束
				Result.Text = FString::Printf(TEXT("%s: 任意"), *Label);
			}
			return Result;
		}

		if (ColumnType.EndsWith(TEXT("FloatRangeColumn")))
		{
			Result.bIsFilterColumn = true;
			const TSharedPtr<FJsonObject> ValueObject = ParseJsonObject(ValueJson);
			if (!ValueObject.IsValid())
			{
				return Result;
			}

			bool bNoMin = false;
			bool bNoMax = false;
			ValueObject->TryGetBoolField(TEXT("bNoMin"), bNoMin);
			ValueObject->TryGetBoolField(TEXT("bNoMax"), bNoMax);
			double MinValue = 0.0;
			double MaxValue = 0.0;
			ValueObject->TryGetNumberField(TEXT("min"), MinValue);
			ValueObject->TryGetNumberField(TEXT("max"), MaxValue);
			const FString MinText = FString::SanitizeFloat(MinValue, 0);
			const FString MaxText = FString::SanitizeFloat(MaxValue, 0);

			if (bNoMin && bNoMax)
			{
				Result.Text = FString::Printf(TEXT("%s: 任意"), *Label);
			}
			else if (bNoMin)
			{
				Result.Text = FString::Printf(TEXT("%s <= %s"), *Label, *MaxText);
				Result.bConstrains = true;
			}
			else if (bNoMax)
			{
				Result.Text = FString::Printf(TEXT("%s >= %s"), *Label, *MinText);
				Result.bConstrains = true;
			}
			else
			{
				Result.Text = FString::Printf(TEXT("%s <= %s <= %s"), *MinText, *Label, *MaxText);
				Result.bConstrains = true;
			}
			return Result;
		}

		// 输出列（OutputStructColumn）及其它非筛选列：不产出筛选条件
		return Result;
	}
	// @KUROGAMES END

	FChooserToolsetRowInfo DescribeRow(const UChooserTable* Chooser, int32 RowIndex, const TArray<FChooserToolsetColumnInfo>& Columns)
	{
		FChooserToolsetRowInfo Info;
		Info.Index = RowIndex;
		Info.bDisabled = Chooser->DisabledRows.IsValidIndex(RowIndex) && Chooser->DisabledRows[RowIndex];

		// @KUROGAMES BEGIN 行筛选条件可读化：收集本行各筛选列的有效条件，拼成行级 ConditionSummary
		TArray<FString> ConstraintTexts;
		// @KUROGAMES END
		for (const FChooserToolsetColumnInfo& Column : Columns)
		{
			FChooserToolsetCellInfo Cell;
			Cell.ColumnIndex = Column.Index;
			Cell.ColumnType = Column.Type;
			Cell.InputType = Column.InputType;
			Cell.BindingDisplayName = Column.Binding.DisplayName;
			Cell.BindingPropertyPath = Column.Binding.PropertyPath;
			if (Column.RowValuesJson.IsValidIndex(RowIndex))
			{
				Cell.ValueJson = Column.RowValuesJson[RowIndex];
			}
			// @KUROGAMES BEGIN 行筛选条件可读化：把该 cell 翻译成可读条件，并收集有效约束用于行汇总
			const FCellConditionResult CellCondition = BuildCellCondition(Cell.ColumnType, Cell.BindingDisplayName, Cell.ValueJson);
			Cell.ConditionText = CellCondition.Text;
			if (CellCondition.bConstrains)
			{
				ConstraintTexts.Add(CellCondition.Text);
			}
			// @KUROGAMES END
			Info.Cells.Add(MoveTemp(Cell));
		}
		// @KUROGAMES BEGIN 行筛选条件可读化：拼出行级汇总；本行无任何有效约束时给出占位说明
		Info.ConditionSummary = ConstraintTexts.Num() > 0
			? FString::Join(ConstraintTexts, TEXT(" 且 "))
			: TEXT("（无筛选条件，命中任意输入）");
		// @KUROGAMES END

		if (Chooser->ResultsStructs.IsValidIndex(RowIndex))
		{
			Info.ResultType = StructTypeName(Chooser->ResultsStructs[RowIndex]);
			Info.ResultJson = StructToJson(Chooser->ResultsStructs[RowIndex]);
			Info.ReferencedObject = GetReferencedObjectPath(Chooser->ResultsStructs[RowIndex]);
			TryExtractNestedChooserPathFromJson(Info.ResultJson, Info.NestedChooserPath);
		}
		return Info;
	}

	FString NormalizeChooserReference(FString ChooserReference)
	{
		ChooserReference = StripChooserObjectPathSyntax(ChooserReference);
		ChooserReference.TrimStartAndEndInline();
		return ChooserReference;
	}

	bool TryExtractNestedChooserPathFromJson(const FString& ResultJson, FString& OutChooserPath)
	{
		const TSharedPtr<FJsonObject> JsonObject = ParseJsonObject(ResultJson);
		if (!JsonObject.IsValid())
		{
			return false;
		}

		FString ChooserPath;
		if (!JsonObject->TryGetStringField(TEXT("chooser"), ChooserPath) || ChooserPath.IsEmpty())
		{
			return false;
		}

		OutChooserPath = NormalizeChooserReference(ChooserPath);
		return !OutChooserPath.IsEmpty();
	}

	struct FNestedChooserReference
	{
		FString ChooserPath;
		int32 RowIndex = INDEX_NONE;
		FString ResultType;
		FString SourceKind;
	};

	void AddNestedChooserReference(int32 RowIndex, const FString& SourceKind, const FString& ResultType, const FString& ResultJson, TArray<FNestedChooserReference>& OutReferences)
	{
		if (!ResultType.EndsWith(TEXT("NestedChooser")))
		{
			return;
		}

		FString NestedChooserPath;
		if (TryExtractNestedChooserPathFromJson(ResultJson, NestedChooserPath))
		{
			FNestedChooserReference& Reference = OutReferences.AddDefaulted_GetRef();
			Reference.ChooserPath = NestedChooserPath;
			Reference.RowIndex = RowIndex;
			Reference.ResultType = ResultType;
			Reference.SourceKind = SourceKind;
		}
	}

	TArray<FNestedChooserReference> FindNestedChooserReferences(const FChooserToolsetDescription& Description)
	{
		TArray<FNestedChooserReference> References;
		for (const FChooserToolsetRowInfo& Row : Description.Rows)
		{
			AddNestedChooserReference(Row.Index, TEXT("RowResult"), Row.ResultType, Row.ResultJson, References);
		}
		AddNestedChooserReference(INDEX_NONE, TEXT("FallbackResult"), Description.FallbackResultType, Description.FallbackResultJson, References);
		return References;
	}

	int32 AppendNestedChooserNode(
		FChooserToolsetNestedChooserDescription& NestedChoosers,
		const FString& RootAssetPath,
		const FString& ChooserPath,
		int32 ParentIndex,
		int32 Depth,
		int32 SourceRowIndex,
		const FString& SourceKind,
		const FString& SourceResultType,
		TSet<FString>& ActivePath)
	{
		const FString NormalizedChooserPath = NormalizeChooserReference(ChooserPath);
		const int32 NodeIndex = NestedChoosers.Nodes.Num();
		FChooserToolsetNestedChooserNode& Node = NestedChoosers.Nodes.AddDefaulted_GetRef();
		Node.Index = NodeIndex;
		Node.ParentIndex = ParentIndex;
		Node.Depth = Depth;
		Node.SourceRowIndex = SourceRowIndex;
		Node.SourceKind = SourceKind;
		Node.SourceResultType = SourceResultType;
		Node.ChooserPath = NormalizedChooserPath;

		if (ParentIndex != INDEX_NONE && NestedChoosers.Nodes.IsValidIndex(ParentIndex))
		{
			NestedChoosers.Nodes[ParentIndex].ChildIndices.Add(NodeIndex);
		}

		if (ActivePath.Contains(NormalizedChooserPath))
		{
			Node.bCycle = true;
			return NodeIndex;
		}

		ActivePath.Add(NormalizedChooserPath);
		Node.Description = UChooserToolset::DescribeChooser(RootAssetPath, NormalizedChooserPath);
		if (Node.Description.OutputObjectType.IsEmpty() && Node.Description.Rows.IsEmpty() && Node.Description.Columns.IsEmpty())
		{
			NestedChoosers.Errors.Add(FString::Printf(TEXT("Failed to describe nested chooser '%s'."), *NormalizedChooserPath));
			ActivePath.Remove(NormalizedChooserPath);
			return NodeIndex;
		}

		for (const FNestedChooserReference& Reference : FindNestedChooserReferences(Node.Description))
		{
			AppendNestedChooserNode(NestedChoosers, RootAssetPath, Reference.ChooserPath, NodeIndex, Depth + 1, Reference.RowIndex, Reference.SourceKind, Reference.ResultType, ActivePath);
		}
		ActivePath.Remove(NormalizedChooserPath);
		return NodeIndex;
	}

	FString StructMemoryToJson(const UScriptStruct* StructType, const void* StructMemory)
	{
		if (!StructType || !StructMemory)
		{
			return FString();
		}

		FString Json;
		FJsonObjectConverter::UStructToJsonObjectString(StructType, StructMemory, Json, 0, 0, 0, nullptr, false);
		return Json;
	}

	FString PropertyValueToJson(FProperty* Property, const void* Value)
	{
		TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, Value, 0, 0);
		if (!JsonValue.IsValid())
		{
			return FString();
		}

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(JsonValue.ToSharedRef(), TEXT(""), Writer);
		return Json;
	}

	FString GetReferencedObjectPath(const FInstancedStruct& Struct)
	{
		const FObjectChooserBase* ChooserBase = Struct.GetPtr<FObjectChooserBase>();
		if (ChooserBase)
		{
			if (UObject* ReferencedObject = ChooserBase->GetReferencedObject())
			{
				return ReferencedObject->GetPathName();
			}
		}

		const UScriptStruct* ScriptStruct = Struct.GetScriptStruct();
		if (!ScriptStruct || !Struct.GetMemory())
		{
			return FString();
		}

		for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
		{
			FProperty* Property = *It;
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				if (UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue_InContainer(Struct.GetMemory()))
				{
					return ObjectValue->GetPathName();
				}
			}
			if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
			{
				const FSoftObjectPtr* SoftObjectValue = SoftObjectProperty->GetPropertyValuePtr_InContainer(Struct.GetMemory());
				if (SoftObjectValue && !SoftObjectValue->IsNull())
				{
					return SoftObjectValue->ToSoftObjectPath().ToString();
				}
			}
		}

		return FString();
	}

	FChooserToolsetColumnSummary SummarizeColumn(const FInstancedStruct& ColumnStruct, int32 ColumnIndex, int32 RowCount, int32 MaxCellSamples)
	{
		FChooserToolsetColumnSummary Summary;
		Summary.Index = ColumnIndex;
		Summary.Type = StructTypeName(ColumnStruct);
		if (const UScriptStruct* ScriptStruct = ColumnStruct.GetScriptStruct())
		{
			Summary.DisplayName = ScriptStruct->GetDisplayNameText().ToString();
		}

		FChooserColumnBase* Column = const_cast<FInstancedStruct&>(ColumnStruct).GetMutablePtr<FChooserColumnBase>();
		if (!Column)
		{
			return Summary;
		}

		Summary.bDisabled = Column->bDisabled;
		Summary.InputType = Column->GetInputType() ? Column->GetInputType()->GetPathName() : FString();
		Summary.RowValuesProperty = Column->RowValuesPropertyName().ToString();

		if (MaxCellSamples <= 0 || Summary.RowValuesProperty.IsEmpty())
		{
			return Summary;
		}

		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(
			ColumnStruct.GetScriptStruct()->FindPropertyByName(Column->RowValuesPropertyName()));
		if (!ArrayProperty)
		{
			return Summary;
		}

		FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ColumnStruct.GetMemory()));
		const int32 ValueCount = FMath::Min3(RowCount, Helper.Num(), MaxCellSamples);
		for (int32 RowIndex = 0; RowIndex < ValueCount; ++RowIndex)
		{
			Summary.SampleRowValuesJson.Add(PropertyValueToJson(ArrayProperty->Inner, Helper.GetRawPtr(RowIndex)));
		}
		return Summary;
	}

	FChooserToolsetRowSummary SummarizeRow(const UChooserTable* Chooser, int32 RowIndex)
	{
		FChooserToolsetRowSummary Summary;
		Summary.Index = RowIndex;
		Summary.bDisabled = Chooser->DisabledRows.IsValidIndex(RowIndex) && Chooser->DisabledRows[RowIndex];
		if (Chooser->ResultsStructs.IsValidIndex(RowIndex))
		{
			Summary.ResultType = StructTypeName(Chooser->ResultsStructs[RowIndex]);
			Summary.ReferencedObject = GetReferencedObjectPath(Chooser->ResultsStructs[RowIndex]);
		}
		return Summary;
	}

	FString IteratorStatusToString(FObjectChooserBase::EIteratorStatus Status)
	{
		switch (Status)
		{
		case FObjectChooserBase::EIteratorStatus::Continue:
			return TEXT("Continue");
		case FObjectChooserBase::EIteratorStatus::ContinueWithOutputs:
			return TEXT("ContinueWithOutputs");
		case FObjectChooserBase::EIteratorStatus::Stop:
			return TEXT("Stop");
		default:
			return TEXT("Unknown");
		}
	}

	bool MatchesOutputType(const UClass* Class, const FString& OutputObjectType)
	{
		return OutputObjectType.IsEmpty() ||
			(Class && (Class->GetPathName().Contains(OutputObjectType) || Class->GetName().Contains(OutputObjectType)));
	}

	FChooserToolsetProxyTableEntryInfo MakeProxyEntryInfo(int32 Index, const FString& SourceTable, const FGuid& Guid, UProxyAsset* ProxyAsset, FName LegacyKey, const FInstancedStruct& ValueStruct, int32 OutputStructCount, bool bIncludeValueJson)
	{
		FChooserToolsetProxyTableEntryInfo Info;
		Info.Index = Index;
		Info.SourceTable = SourceTable;
		Info.ProxyAsset = ProxyAsset ? ProxyAsset->GetPathName() : FString();
		Info.Guid = Guid.ToString(EGuidFormats::DigitsWithHyphens);
		Info.LegacyKey = LegacyKey.IsNone() ? FString() : LegacyKey.ToString();
		Info.ValueType = StructTypeName(ValueStruct);
		Info.ReferencedObject = GetReferencedObjectPath(ValueStruct);
		Info.OutputStructCount = OutputStructCount;
		if (bIncludeValueJson)
		{
			Info.ValueJson = StructToJson(ValueStruct);
		}
		return Info;
	}

	FChooserToolsetProxyTableEntryInfo MakeProxyEntryInfo(int32 Index, const UProxyTable* ProxyTable, const FProxyEntry& Entry, bool bIncludeValueJson)
	{
		return MakeProxyEntryInfo(
			Index,
			ProxyTable ? ProxyTable->GetPathName() : FString(),
			Entry.GetGuid(),
			Entry.Proxy,
			Entry.Key,
			Entry.ValueStruct,
			Entry.OutputStructData.Num(),
			bIncludeValueJson);
	}

	FChooserToolsetProxyTableEntryInfo MakeRuntimeProxyEntryInfo(int32 Index, const UProxyTable* ProxyTable, bool bIncludeValueJson)
	{
		const FRuntimeProxyValue& RuntimeValue = ProxyTable->RuntimeValues[Index];
		const FGuid Guid = ProxyTable->Keys.IsValidIndex(Index) ? ProxyTable->Keys[Index] : FGuid();
		return MakeProxyEntryInfo(
			Index,
			ProxyTable ? ProxyTable->GetPathName() : FString(),
			Guid,
			RuntimeValue.ProxyAsset,
			NAME_None,
			RuntimeValue.Value,
			RuntimeValue.OutputStructData.Num(),
			bIncludeValueJson);
	}
}

TArray<FChooserToolsetChooserRef> UChooserToolset::ListChoosers(const FString& OutputObjectType)
{
	TArray<FChooserToolsetChooserRef> Result;

	TArray<FAssetData> Assets;
	IAssetRegistry::Get()->GetAssetsByClass(UChooserTable::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		UChooserTable* Chooser = Cast<UChooserTable>(Asset.GetAsset());
		if (!Chooser)
		{
			continue;
		}

		const FString OutputType = Chooser->OutputObjectType ? Chooser->OutputObjectType->GetPathName() : FString();
		if (!OutputObjectType.IsEmpty() && !OutputType.Contains(OutputObjectType) &&
			(!Chooser->OutputObjectType || !Chooser->OutputObjectType->GetName().Contains(OutputObjectType)))
		{
			continue;
		}

		FChooserToolsetChooserRef Ref;
		Ref.AssetPath = Asset.GetSoftObjectPath().ToString();
		Ref.OutputObjectType = OutputType;
		Ref.ResultType = ResultTypeToString(Chooser->ResultType);
		Result.Add(Ref);
	}

	Result.Sort([](const FChooserToolsetChooserRef& A, const FChooserToolsetChooserRef& B)
	{
		return A.AssetPath < B.AssetPath;
	});
	return Result;
}

FChooserToolsetDescription UChooserToolset::DescribeChooser(const FString& AssetPath, const FString& NestedChooserName)
{
	FChooserToolsetDescription Description;
	Description.AssetPath = AssetPath;
	Description.NestedChooserName = NestedChooserName;

	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser)
	{
		return Description;
	}

	Description.OutputObjectType = Chooser->OutputObjectType ? Chooser->OutputObjectType->GetPathName() : FString();
	Description.ResultType = ResultTypeToString(Chooser->ResultType);
	for (UChooserTable* NestedChooser : Chooser->GetRootChooser()->NestedChoosers)
	{
		if (NestedChooser)
		{
			Description.NestedChooserNames.Add(NestedChooser->GetName());
		}
	}

	const UChooserTable* ContextOwner = Chooser->GetContextOwner();
	for (int32 Index = 0; Index < ContextOwner->ContextData.Num(); ++Index)
	{
		FChooserToolsetContextInfo ContextInfo;
		ContextInfo.Index = Index;
		ContextInfo.Type = StructTypeName(ContextOwner->ContextData[Index]);
		ContextInfo.ValueJson = StructToJson(ContextOwner->ContextData[Index]);
		Description.Context.Add(ContextInfo);
	}

	const int32 RowCount = GetRowCount(Chooser);
	for (int32 ColumnIndex = 0; ColumnIndex < Chooser->ColumnsStructs.Num(); ++ColumnIndex)
	{
		Description.Columns.Add(DescribeColumn(Chooser->ColumnsStructs[ColumnIndex], ColumnIndex, RowCount));
	}
	for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
	{
		Description.Rows.Add(DescribeRow(Chooser, RowIndex, Description.Columns));
	}

	Description.FallbackResultType = StructTypeName(Chooser->FallbackResult);
	Description.FallbackResultJson = StructToJson(Chooser->FallbackResult);
	Description.FallbackReferencedObject = GetReferencedObjectPath(Chooser->FallbackResult);
	TryExtractNestedChooserPathFromJson(Description.FallbackResultJson, Description.FallbackNestedChooserPath);
	return Description;
}

FChooserToolsetNestedChooserDescription UChooserToolset::DescribeNestedChoosers(const FString& AssetPath, const FString& NestedChooserName)
{
	FChooserToolsetNestedChooserDescription NestedChoosers;
	NestedChoosers.AssetPath = AssetPath;
	NestedChoosers.NestedChooserName = NestedChooserName;

	UChooserTable* RootChooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!RootChooser)
	{
		NestedChoosers.Errors.Add(FString::Printf(TEXT("Chooser asset '%s' could not be loaded."), *AssetPath));
		return NestedChoosers;
	}

	const FString RootPath = RootChooser->GetRootChooser()->GetPathName();
	const FString StartPath = RootChooser->GetPathName();
	TSet<FString> ActivePath;
	AppendNestedChooserNode(NestedChoosers, RootPath, StartPath, INDEX_NONE, 0, INDEX_NONE, TEXT("Root"), FString(), ActivePath);
	return NestedChoosers;
}

FChooserToolsetSummary UChooserToolset::DescribeChooserSummary(const FString& AssetPath, const FString& NestedChooserName, int32 MaxRows, int32 MaxCellSamples)
{
	FChooserToolsetSummary Summary;
	Summary.AssetPath = AssetPath;
	Summary.NestedChooserName = NestedChooserName;

	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser)
	{
		return Summary;
	}

	Summary.ResolvedChooserPath = Chooser->GetPathName();
	Summary.OutputObjectType = Chooser->OutputObjectType ? Chooser->OutputObjectType->GetPathName() : FString();
	Summary.ResultType = ResultTypeToString(Chooser->ResultType);
	Summary.RowCount = GetRowCount(Chooser);
	Summary.ColumnCount = Chooser->ColumnsStructs.Num();

	const UChooserTable* ContextOwner = Chooser->GetContextOwner();
	for (const FInstancedStruct& ContextStruct : ContextOwner->ContextData)
	{
		Summary.ContextTypes.Add(StructTypeName(ContextStruct));
	}

	for (UChooserTable* NestedChooser : Chooser->NestedChoosers)
	{
		if (NestedChooser)
		{
			Summary.DirectNestedChooserNames.Add(NestedChooser->GetName());
		}
	}

	for (int32 ColumnIndex = 0; ColumnIndex < Chooser->ColumnsStructs.Num(); ++ColumnIndex)
	{
		Summary.Columns.Add(SummarizeColumn(Chooser->ColumnsStructs[ColumnIndex], ColumnIndex, Summary.RowCount, MaxCellSamples));
	}

	const int32 RowsToInclude = MaxRows <= 0 ? Summary.RowCount : FMath::Min(Summary.RowCount, MaxRows);
	for (int32 RowIndex = 0; RowIndex < RowsToInclude; ++RowIndex)
	{
		Summary.Rows.Add(SummarizeRow(Chooser, RowIndex));
	}

	Summary.FallbackResultType = StructTypeName(Chooser->FallbackResult);
	return Summary;
}

TArray<FChooserToolsetStructTypeInfo> UChooserToolset::ListAvailableColumnTypes()
{
	return ListStructTypes(FChooserColumnBase::StaticStruct());
}

TArray<FChooserToolsetStructTypeInfo> UChooserToolset::ListAvailableResultTypes()
{
	return ListStructTypes(FObjectChooserBase::StaticStruct());
}

TArray<FString> UChooserToolset::ListContextProperties(const FString& TypePath)
{
	UStruct* Struct = FindClassType(TypePath);
	if (!Struct)
	{
		Struct = FindStructType(TypePath);
	}
	if (!Struct)
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Context type '%s' was not found."), *TypePath));
		return TArray<FString>();
	}

	TArray<FString> Result;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		Result.Add(FString::Printf(TEXT("%s: %s"), *Property->GetName(), *Property->GetCPPType()));
	}
	Result.Sort();
	return Result;
}

FString UChooserToolset::CreateChooser(const FString& PackagePath, const FString& ResultType, const FString& OutputObjectType)
{
	EObjectChooserResultType ParsedResultType = EObjectChooserResultType::ObjectResult;
	if (!ParseResultType(ResultType, ParsedResultType))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Invalid chooser result type '%s'."), *ResultType));
		return FString();
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "CreateChooser", "Create Chooser"));
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Could not create package '%s'."), *PackagePath));
		return FString();
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	UChooserTable* Chooser = NewObject<UChooserTable>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Chooser->Modify();
	Chooser->ResultType = ParsedResultType;
	Chooser->OutputObjectType = FindClassType(OutputObjectType);
	Chooser->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Chooser);
	return Chooser->GetPathName();
}

void UChooserToolset::SetContextData(const FString& AssetPath, const FString& ContextDataJson)
{
	UChooserTable* RootChooser = LoadRootChooser(AssetPath);
	if (!RootChooser)
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>> ContextArray = ParseJsonArray(ContextDataJson);
	if (ContextArray.IsEmpty() && !ContextDataJson.TrimStartAndEnd().Equals(TEXT("[]")))
	{
		RaiseChooserToolsetError(TEXT("ContextDataJson must be a JSON array."));
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "SetContextData", "Set Chooser Context Data"));
	RootChooser->Modify();
	RootChooser->ContextData.Reset();

	for (const TSharedPtr<FJsonValue>& Value : ContextArray)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjectPtr) || !ObjectPtr || !ObjectPtr->IsValid())
		{
			RaiseChooserToolsetError(TEXT("Each context entry must be a JSON object."));
			return;
		}

		FString Type;
		if (!(*ObjectPtr)->TryGetStringField(TEXT("Type"), Type))
		{
			RaiseChooserToolsetError(TEXT("Each context entry must contain a Type string."));
			return;
		}

		FString ValueJson = TEXT("{}");
		if ((*ObjectPtr)->HasField(TEXT("Value")))
		{
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueJson);
			FJsonSerializer::Serialize((*ObjectPtr)->TryGetField(TEXT("Value")).ToSharedRef(), TEXT(""), Writer);
		}

		FInstancedStruct ContextStruct;
		if (!FillInstancedStructFromJson(ContextStruct, Type, ValueJson, nullptr))
		{
			return;
		}
		RootChooser->ContextData.Add(MoveTemp(ContextStruct));
	}

	FPropertyChangedEvent PropertyChangedEvent(FindFProperty<FProperty>(UChooserTable::StaticClass(), GET_MEMBER_NAME_CHECKED(UChooserSignature, ContextData)));
	RootChooser->PostEditChangeProperty(PropertyChangedEvent);
	MarkChooserChanged(RootChooser);
}

int32 UChooserToolset::AddColumn(const FString& AssetPath, const FString& NestedChooserName, const FString& ColumnType, const FString& InputType)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser)
	{
		return INDEX_NONE;
	}

	FInstancedStruct ColumnStruct;
	if (!FillInstancedStructFromJson(ColumnStruct, ColumnType, TEXT("{}"), FChooserColumnBase::StaticStruct()))
	{
		return INDEX_NONE;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "AddColumn", "Add Chooser Column"));
	Chooser->Modify();
	FChooserColumnBase* Column = ColumnStruct.GetMutablePtr<FChooserColumnBase>();
	Column->Initialize(Chooser);
	if (!InputType.IsEmpty())
	{
		UScriptStruct* InputStruct = FindStructType(InputType, Column->GetInputBaseType());
		if (!InputStruct)
		{
			RaiseChooserToolsetError(FString::Printf(TEXT("Input type '%s' is not valid for column '%s'."), *InputType, *ColumnType));
			return INDEX_NONE;
		}
		Column->SetInputType(InputStruct);
	}
	Column->SetNumRows(GetRowCount(Chooser));

	const int32 NewIndex = Chooser->ColumnsStructs.Add(MoveTemp(ColumnStruct));
	MarkChooserChanged(Chooser);
	return NewIndex;
}

void UChooserToolset::RemoveColumn(const FString& AssetPath, const FString& NestedChooserName, int32 ColumnIndex)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser || !Chooser->ColumnsStructs.IsValidIndex(ColumnIndex))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Column index %d is out of range."), ColumnIndex));
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "RemoveColumn", "Remove Chooser Column"));
	Chooser->Modify();
	Chooser->ColumnsStructs.RemoveAt(ColumnIndex);
	MarkChooserChanged(Chooser);
}

int32 UChooserToolset::AddRow(const FString& AssetPath, const FString& NestedChooserName)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser)
	{
		return INDEX_NONE;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "AddRow", "Add Chooser Row"));
	Chooser->Modify();
	const int32 RowIndex = Chooser->ResultsStructs.AddDefaulted();
	Chooser->DisabledRows.SetNum(Chooser->ResultsStructs.Num());
	for (FInstancedStruct& ColumnStruct : Chooser->ColumnsStructs)
	{
		if (FChooserColumnBase* Column = ColumnStruct.GetMutablePtr<FChooserColumnBase>())
		{
			Column->InsertRows(RowIndex, 1);
		}
	}
	MarkChooserChanged(Chooser);
	return RowIndex;
}

void UChooserToolset::RemoveRow(const FString& AssetPath, const FString& NestedChooserName, int32 RowIndex)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser || !Chooser->ResultsStructs.IsValidIndex(RowIndex))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Row index %d is out of range."), RowIndex));
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "RemoveRow", "Remove Chooser Row"));
	Chooser->Modify();
	Chooser->ResultsStructs.RemoveAt(RowIndex);
	if (Chooser->DisabledRows.IsValidIndex(RowIndex))
	{
		Chooser->DisabledRows.RemoveAt(RowIndex);
	}
	const TArray<uint32> RowsToDelete{ static_cast<uint32>(RowIndex) };
	for (FInstancedStruct& ColumnStruct : Chooser->ColumnsStructs)
	{
		if (FChooserColumnBase* Column = ColumnStruct.GetMutablePtr<FChooserColumnBase>())
		{
			Column->DeleteRows(RowsToDelete);
		}
	}
	MarkChooserChanged(Chooser);
}

void UChooserToolset::MoveRow(const FString& AssetPath, const FString& NestedChooserName, int32 SourceRowIndex, int32 TargetRowIndex)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser || !Chooser->ResultsStructs.IsValidIndex(SourceRowIndex) || TargetRowIndex < 0 || TargetRowIndex > Chooser->ResultsStructs.Num())
	{
		RaiseChooserToolsetError(TEXT("Source or target row index is out of range."));
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "MoveRow", "Move Chooser Row"));
	Chooser->Modify();
	FInstancedStruct Row = MoveTemp(Chooser->ResultsStructs[SourceRowIndex]);
	Chooser->ResultsStructs.RemoveAt(SourceRowIndex);
	int32 InsertIndex = TargetRowIndex;
	if (SourceRowIndex < InsertIndex)
	{
		--InsertIndex;
	}
	Chooser->ResultsStructs.Insert(MoveTemp(Row), InsertIndex);

	if (Chooser->DisabledRows.IsValidIndex(SourceRowIndex))
	{
		const bool bDisabled = Chooser->DisabledRows[SourceRowIndex];
		Chooser->DisabledRows.RemoveAt(SourceRowIndex);
		Chooser->DisabledRows.Insert(bDisabled, InsertIndex);
	}

	for (FInstancedStruct& ColumnStruct : Chooser->ColumnsStructs)
	{
		if (FChooserColumnBase* Column = ColumnStruct.GetMutablePtr<FChooserColumnBase>())
		{
			Column->MoveRow(SourceRowIndex, TargetRowIndex);
		}
	}
	MarkChooserChanged(Chooser);
}

void UChooserToolset::SetCell(const FString& AssetPath, const FString& NestedChooserName, int32 ColumnIndex, int32 RowIndex, const FString& CellJson)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	FChooserColumnBase* Column = GetColumn(Chooser, ColumnIndex);
	if (!Chooser || !Column || !Chooser->ResultsStructs.IsValidIndex(RowIndex))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Row index %d is out of range."), RowIndex));
		return;
	}

	FInstancedStruct& ColumnStruct = Chooser->ColumnsStructs[ColumnIndex];
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(
		ColumnStruct.GetScriptStruct()->FindPropertyByName(Column->RowValuesPropertyName()));
	if (!ArrayProperty)
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Column %d does not expose a row values array."), ColumnIndex));
		return;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ColumnStruct.GetMutableMemory()));
	if (!Helper.IsValidIndex(RowIndex))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Cell row %d is out of range for column %d."), RowIndex, ColumnIndex));
		return;
	}

	TSharedPtr<FJsonValue> JsonValue = ParseJsonValue(CellJson);
	if (!JsonValue.IsValid())
	{
		RaiseChooserToolsetError(TEXT("CellJson must be a valid JSON value."));
		return;
	}

	FText FailReason;
	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "SetCell", "Set Chooser Cell"));
	Chooser->Modify();
	if (!FJsonObjectConverter::JsonValueToUProperty(JsonValue, ArrayProperty->Inner, Helper.GetRawPtr(RowIndex), 0, 0, false, &FailReason))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Failed to set cell: %s"), *FailReason.ToString()));
		return;
	}
	MarkChooserChanged(Chooser);
}

void UChooserToolset::SetRowResult(const FString& AssetPath, const FString& NestedChooserName, int32 RowIndex, const FString& ResultType, const FString& ResultJson)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser || !Chooser->ResultsStructs.IsValidIndex(RowIndex))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Row index %d is out of range."), RowIndex));
		return;
	}

	FInstancedStruct ResultStruct;
	if (!FillInstancedStructFromJson(ResultStruct, ResultType, ResultJson, FObjectChooserBase::StaticStruct()))
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "SetRowResult", "Set Chooser Row Result"));
	Chooser->Modify();
	Chooser->ResultsStructs[RowIndex] = MoveTemp(ResultStruct);
	MarkChooserChanged(Chooser);
}

void UChooserToolset::SetFallback(const FString& AssetPath, const FString& NestedChooserName, const FString& ResultType, const FString& ResultJson)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser)
	{
		return;
	}

	FInstancedStruct ResultStruct;
	if (!FillInstancedStructFromJson(ResultStruct, ResultType, ResultJson, FObjectChooserBase::StaticStruct()))
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "SetFallback", "Set Chooser Fallback"));
	Chooser->Modify();
	Chooser->FallbackResult = MoveTemp(ResultStruct);
	MarkChooserChanged(Chooser);
}

void UChooserToolset::SetRowDisabled(const FString& AssetPath, const FString& NestedChooserName, int32 RowIndex, bool bDisabled)
{
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser || !Chooser->ResultsStructs.IsValidIndex(RowIndex))
	{
		RaiseChooserToolsetError(FString::Printf(TEXT("Row index %d is out of range."), RowIndex));
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ChooserToolset", "SetRowDisabled", "Set Chooser Row Disabled"));
	Chooser->Modify();
	Chooser->DisabledRows.SetNum(Chooser->ResultsStructs.Num());
	Chooser->DisabledRows[RowIndex] = bDisabled;
	MarkChooserChanged(Chooser);
}

FChooserToolsetCompileResult UChooserToolset::CompileChooser(const FString& AssetPath, const FString& NestedChooserName)
{
	FChooserToolsetCompileResult Result;
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser)
	{
		Result.Messages.Add(TEXT("Chooser could not be loaded."));
		return Result;
	}

	Chooser->Compile(true);
	Result.bCompiled = true;

	FDataValidationContext ValidationContext;
	Result.bValid = static_cast<const UChooserTable*>(Chooser)->IsDataValid(ValidationContext) != EDataValidationResult::Invalid;
	for (const FDataValidationContext::FIssue& Issue : ValidationContext.GetIssues())
	{
		Result.Messages.Add(Issue.Message.ToString());
	}
	return Result;
}

FChooserToolsetEvaluationResult UChooserToolset::TestEvaluate(const FString& AssetPath, const FString& NestedChooserName, const FString& ContextJson)
{
	FChooserToolsetEvaluationResult Result;
	UChooserTable* Chooser = ResolveChooser(AssetPath, NestedChooserName);
	if (!Chooser)
	{
		Result.Messages.Add(TEXT("Chooser could not be loaded."));
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>> ContextArray = ParseJsonArray(ContextJson);
	if (ContextArray.IsEmpty() && !ContextJson.TrimStartAndEnd().Equals(TEXT("[]")))
	{
		Result.Messages.Add(TEXT("ContextJson must be a JSON array."));
		return Result;
	}

	FChooserEvaluationContext Context;
	TArray<TObjectPtr<UObject>> ObjectKeepAlive;
	TArray<FInstancedStruct> StructKeepAlive;

	for (const TSharedPtr<FJsonValue>& Value : ContextArray)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjectPtr) || !ObjectPtr || !ObjectPtr->IsValid())
		{
			Result.Messages.Add(TEXT("Each context entry must be a JSON object."));
			return Result;
		}

		FString ObjectPath;
		if ((*ObjectPtr)->TryGetStringField(TEXT("ObjectPath"), ObjectPath))
		{
			UObject* Object = LoadObjectFromPath(ObjectPath);
			if (!Object)
			{
				Result.Messages.Add(FString::Printf(TEXT("Object context '%s' could not be loaded."), *ObjectPath));
				return Result;
			}
			ObjectKeepAlive.Add(Object);
			Context.AddObjectParam(Object);
			continue;
		}

		FString Type;
		if (!(*ObjectPtr)->TryGetStringField(TEXT("Type"), Type))
		{
			Result.Messages.Add(TEXT("Context entries must contain ObjectPath or Type."));
			return Result;
		}

		FString ValueJson = TEXT("{}");
		if ((*ObjectPtr)->HasField(TEXT("Value")))
		{
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueJson);
			FJsonSerializer::Serialize((*ObjectPtr)->TryGetField(TEXT("Value")).ToSharedRef(), TEXT(""), Writer);
		}

		FInstancedStruct Struct;
		if (!FillInstancedStructFromJson(Struct, Type, ValueJson, nullptr))
		{
			Result.Messages.Add(FString::Printf(TEXT("Failed to build struct context '%s'."), *Type));
			return Result;
		}

		StructKeepAlive.Add(MoveTemp(Struct));
		Context.AddStructViewParam(FStructView(StructKeepAlive.Last().GetScriptStruct(), StructKeepAlive.Last().GetMutableMemory()));
	}

	Chooser->Compile(false);
	FObjectChooserBase::EIteratorStatus Status = UChooserTable::EvaluateChooser(
		Context,
		Chooser,
		FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
			[&Result](UObject* SelectedObject)
			{
				if (SelectedObject)
				{
					Result.SelectedObjects.Add(SelectedObject->GetPathName());
				}
				return FObjectChooserBase::EIteratorStatus::Continue;
			}));

	Result.Status = IteratorStatusToString(Status);
	Result.bHadOutputs = Status == FObjectChooserBase::EIteratorStatus::ContinueWithOutputs;
	return Result;
}

TArray<FChooserToolsetProxyRef> UChooserToolset::ListProxyTables(const FString& OutputObjectType)
{
	TArray<FChooserToolsetProxyRef> Result;
	TArray<FAssetData> Assets;
	IAssetRegistry::Get()->GetAssetsByClass(UProxyTable::StaticClass()->GetClassPathName(), Assets, true);

	for (const FAssetData& Asset : Assets)
	{
		UProxyTable* ProxyTable = Cast<UProxyTable>(Asset.GetAsset());
		if (!ProxyTable)
		{
			continue;
		}

		bool bMatches = OutputObjectType.IsEmpty();
		if (!bMatches)
		{
			for (const FRuntimeProxyValue& RuntimeValue : ProxyTable->RuntimeValues)
			{
				if (RuntimeValue.ProxyAsset && MatchesOutputType(RuntimeValue.ProxyAsset->Type, OutputObjectType))
				{
					bMatches = true;
					break;
				}
			}
		}
		if (!bMatches)
		{
			continue;
		}

		FChooserToolsetProxyRef Ref;
		Ref.AssetPath = Asset.GetSoftObjectPath().ToString();
		Ref.Type = TEXT("ProxyTable");
		Result.Add(Ref);
	}

	Result.Sort([](const FChooserToolsetProxyRef& A, const FChooserToolsetProxyRef& B)
	{
		return A.AssetPath < B.AssetPath;
	});
	return Result;
}

TArray<FChooserToolsetProxyRef> UChooserToolset::ListProxyAssets(const FString& OutputObjectType)
{
	TArray<FChooserToolsetProxyRef> Result;
	TArray<FAssetData> Assets;
	IAssetRegistry::Get()->GetAssetsByClass(UProxyAsset::StaticClass()->GetClassPathName(), Assets, true);

	for (const FAssetData& Asset : Assets)
	{
		UProxyAsset* ProxyAsset = Cast<UProxyAsset>(Asset.GetAsset());
		if (!ProxyAsset || !MatchesOutputType(ProxyAsset->Type, OutputObjectType))
		{
			continue;
		}

		FChooserToolsetProxyRef Ref;
		Ref.AssetPath = Asset.GetSoftObjectPath().ToString();
		Ref.Type = TEXT("ProxyAsset");
		Ref.OutputObjectType = ProxyAsset->Type ? ProxyAsset->Type->GetPathName() : FString();
		Ref.ResultType = ResultTypeToString(ProxyAsset->ResultType);
		Ref.Guid = ProxyAsset->Guid.ToString(EGuidFormats::DigitsWithHyphens);
		Result.Add(Ref);
	}

	Result.Sort([](const FChooserToolsetProxyRef& A, const FChooserToolsetProxyRef& B)
	{
		return A.AssetPath < B.AssetPath;
	});
	return Result;
}

FChooserToolsetProxyAssetInfo UChooserToolset::DescribeProxyAsset(const FString& AssetPath)
{
	FChooserToolsetProxyAssetInfo Info;
	Info.AssetPath = AssetPath;

	UProxyAsset* ProxyAsset = LoadProxyAsset(AssetPath);
	if (!ProxyAsset)
	{
		return Info;
	}

	Info.AssetPath = ProxyAsset->GetPathName();
	Info.Guid = ProxyAsset->Guid.ToString(EGuidFormats::DigitsWithHyphens);
	Info.OutputObjectType = ProxyAsset->Type ? ProxyAsset->Type->GetPathName() : FString();
	Info.ResultType = ResultTypeToString(ProxyAsset->ResultType);

	for (int32 Index = 0; Index < ProxyAsset->ContextData.Num(); ++Index)
	{
		FChooserToolsetContextInfo ContextInfo;
		ContextInfo.Index = Index;
		ContextInfo.Type = StructTypeName(ProxyAsset->ContextData[Index]);
		ContextInfo.ValueJson = StructToJson(ProxyAsset->ContextData[Index]);
		Info.Context.Add(ContextInfo);
	}
	return Info;
}

FChooserToolsetProxyTableInfo UChooserToolset::DescribeProxyTable(const FString& AssetPath, bool bIncludeValueJson)
{
	FChooserToolsetProxyTableInfo Info;
	Info.AssetPath = AssetPath;

	UProxyTable* ProxyTable = LoadProxyTable(AssetPath);
	if (!ProxyTable)
	{
		return Info;
	}

	Info.AssetPath = ProxyTable->GetPathName();

	for (UProxyTable* ParentTable : ProxyTable->InheritEntriesFrom)
	{
		if (ParentTable)
		{
			Info.InheritedTables.Add(ParentTable->GetPathName());
		}
	}

	TMap<FString, int32> GuidCounts;
	for (int32 Index = 0; Index < ProxyTable->Entries.Num(); ++Index)
	{
		FChooserToolsetProxyTableEntryInfo EntryInfo = MakeProxyEntryInfo(Index, ProxyTable, ProxyTable->Entries[Index], bIncludeValueJson);
		GuidCounts.FindOrAdd(EntryInfo.Guid)++;
		Info.EditorEntries.Add(MoveTemp(EntryInfo));
	}

	const int32 RuntimeCount = FMath::Min(ProxyTable->Keys.Num(), ProxyTable->RuntimeValues.Num());
	for (int32 Index = 0; Index < RuntimeCount; ++Index)
	{
		Info.RuntimeEntries.Add(MakeRuntimeProxyEntryInfo(Index, ProxyTable, bIncludeValueJson));
	}

	for (const TPair<FString, int32>& GuidCount : GuidCounts)
	{
		if (GuidCount.Value > 1)
		{
			Info.DuplicateGuids.Add(GuidCount.Key);
		}
	}
	Info.DuplicateGuids.Sort();
	return Info;
}

TArray<FChooserToolsetProxyTableEntryInfo> UChooserToolset::FindProxyMappings(const FString& ProxyAssetPath, const FString& ProxyTablePath)
{
	TArray<FChooserToolsetProxyTableEntryInfo> Result;
	UProxyAsset* ProxyAsset = LoadProxyAsset(ProxyAssetPath);
	if (!ProxyAsset)
	{
		return Result;
	}

	TArray<UProxyTable*> TablesToSearch;
	if (!ProxyTablePath.IsEmpty())
	{
		if (UProxyTable* ProxyTable = LoadProxyTable(ProxyTablePath))
		{
			TablesToSearch.Add(ProxyTable);
		}
	}
	else
	{
		TArray<FAssetData> Assets;
		IAssetRegistry::Get()->GetAssetsByClass(UProxyTable::StaticClass()->GetClassPathName(), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (UProxyTable* ProxyTable = Cast<UProxyTable>(Asset.GetAsset()))
			{
				TablesToSearch.Add(ProxyTable);
			}
		}
	}

	for (UProxyTable* ProxyTable : TablesToSearch)
	{
		if (!ProxyTable)
		{
			continue;
		}

		const int32 RuntimeCount = FMath::Min(ProxyTable->Keys.Num(), ProxyTable->RuntimeValues.Num());
		for (int32 Index = 0; Index < RuntimeCount; ++Index)
		{
			if (ProxyTable->Keys[Index] == ProxyAsset->Guid)
			{
				Result.Add(MakeRuntimeProxyEntryInfo(Index, ProxyTable, false));
			}
		}
	}

	Result.Sort([](const FChooserToolsetProxyTableEntryInfo& A, const FChooserToolsetProxyTableEntryInfo& B)
	{
		if (A.SourceTable == B.SourceTable)
		{
			return A.Index < B.Index;
		}
		return A.SourceTable < B.SourceTable;
	});
	return Result;
}
