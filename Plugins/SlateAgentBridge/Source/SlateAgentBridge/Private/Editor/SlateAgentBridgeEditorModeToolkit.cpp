// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateAgentBridgeEditorModeToolkit.h"
#include "SlateAgentBridgeEditorMode.h"
#include "Engine/Selection.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "SlateAgentBridgeEditorModeToolkit"

FSlateAgentBridgeEditorModeToolkit::FSlateAgentBridgeEditorModeToolkit()
{
}

void FSlateAgentBridgeEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FSlateAgentBridgeEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FSlateAgentBridgeEditorModeToolkit::GetToolkitFName() const
{
	return FName("SlateAgentBridgeEditorMode");
}

FText FSlateAgentBridgeEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "SlateAgentBridgeEditorMode Toolkit");
}

#undef LOCTEXT_NAMESPACE
