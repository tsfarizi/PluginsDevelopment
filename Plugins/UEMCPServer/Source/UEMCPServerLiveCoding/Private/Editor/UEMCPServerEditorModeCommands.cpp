// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEMCPServerEditorModeCommands.h"
#include "UEMCPServerEditorMode.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "UEMCPServerEditorModeCommands"

FUEMCPServerEditorModeCommands::FUEMCPServerEditorModeCommands()
	: TCommands<FUEMCPServerEditorModeCommands>("UEMCPServerEditorMode",
		NSLOCTEXT("UEMCPServerEditorMode", "UEMCPServerEditorModeCommands", "UE MCP Server Editor Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FUEMCPServerEditorModeCommands::RegisterCommands()
{
	TArray <TSharedPtr<FUICommandInfo>>& ToolCommands = Commands.FindOrAdd(NAME_Default);

	UI_COMMAND(SimpleTool, "Show Actor Info", "Opens message box with info about a clicked actor", EUserInterfaceActionType::Button, FInputChord());
	ToolCommands.Add(SimpleTool);

	UI_COMMAND(InteractiveTool, "Measure Distance", "Measures distance between 2 points (click to set origin, shift-click to set end point)", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(InteractiveTool);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FUEMCPServerEditorModeCommands::GetCommands()
{
	return FUEMCPServerEditorModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
