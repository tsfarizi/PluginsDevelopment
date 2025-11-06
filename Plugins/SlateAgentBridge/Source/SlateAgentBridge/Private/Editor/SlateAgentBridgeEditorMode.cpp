// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateAgentBridgeEditorMode.h"
#include "SlateAgentBridgeEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "SlateAgentBridgeEditorModeCommands.h"
#include "Modules/ModuleManager.h"


//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
// AddYourTool Step 1 - include the header file for your Tools here
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
#include "Tools/SlateAgentBridgeSimpleTool.h"
#include "Tools/SlateAgentBridgeInteractiveTool.h"

// step 2: register a ToolBuilder in FSlateAgentBridgeEditorMode::Enter() below


#define LOCTEXT_NAMESPACE "SlateAgentBridgeEditorMode"

const FEditorModeID USlateAgentBridgeEditorMode::EM_SlateAgentBridgeEditorModeId = TEXT("EM_SlateAgentBridgeEditorMode");

FString USlateAgentBridgeEditorMode::SimpleToolName = TEXT("SlateAgentBridge_ActorInfoTool");
FString USlateAgentBridgeEditorMode::InteractiveToolName = TEXT("SlateAgentBridge_MeasureDistanceTool");


USlateAgentBridgeEditorMode::USlateAgentBridgeEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	// appearance and icon in the editing mode ribbon can be customized here
	Info = FEditorModeInfo(USlateAgentBridgeEditorMode::EM_SlateAgentBridgeEditorModeId,
		LOCTEXT("ModeName", "SlateAgentBridge"),
		FSlateIcon(),
		true);
}


USlateAgentBridgeEditorMode::~USlateAgentBridgeEditorMode()
{
}


void USlateAgentBridgeEditorMode::ActorSelectionChangeNotify()
{
}

void USlateAgentBridgeEditorMode::Enter()
{
	UEdMode::Enter();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// AddYourTool Step 2 - register the ToolBuilders for your Tools here.
	// The string name you pass to the ToolManager is used to select/activate your ToolBuilder later.
	//////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////// 
	const FSlateAgentBridgeEditorModeCommands& SampleToolCommands = FSlateAgentBridgeEditorModeCommands::Get();

	RegisterTool(SampleToolCommands.SimpleTool, SimpleToolName, NewObject<USlateAgentBridgeSimpleToolBuilder>(this));
	RegisterTool(SampleToolCommands.InteractiveTool, InteractiveToolName, NewObject<USlateAgentBridgeInteractiveToolBuilder>(this));

	// active tool type is not relevant here, we just set to default
	GetToolManager()->SelectActiveToolType(EToolSide::Left, SimpleToolName);
}

void USlateAgentBridgeEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FSlateAgentBridgeEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> USlateAgentBridgeEditorMode::GetModeCommands() const
{
	return FSlateAgentBridgeEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
