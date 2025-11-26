// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEMCPServerEditorModeToolkit.h"
#include "UEMCPServerEditorMode.h"
#include "Engine/Selection.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "UEMCPServerEditorModeToolkit"

FUEMCPServerEditorModeToolkit::FUEMCPServerEditorModeToolkit()
{
}

void FUEMCPServerEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FUEMCPServerEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FUEMCPServerEditorModeToolkit::GetToolkitFName() const
{
	return FName("UEMCPServerEditorMode");
}

FText FUEMCPServerEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "UEMCPServerEditorMode Toolkit");
}

#undef LOCTEXT_NAMESPACE
