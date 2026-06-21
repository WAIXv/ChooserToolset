// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserToolset/ChooserToolset.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

class FChooserToolsetModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UChooserToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UChooserToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FChooserToolsetModule, ChooserToolset);
