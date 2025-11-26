// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEMCPServerEditorMode.h"
#include "UEMCPServerEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "UEMCPServerEditorModeCommands.h"
#include "Modules/ModuleManager.h"


//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
// AddYourTool Step 1 - include the header file for your Tools here
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
#include "Tools/UEMCPServerSimpleTool.h"
#include "Tools/UEMCPServerInteractiveTool.h"

// step 2: register a ToolBuilder in FUEMCPServerEditorMode::Enter() below


#define LOCTEXT_NAMESPACE "UEMCPServerEditorMode"

const FEditorModeID UUEMCPServerEditorMode::EM_UEMCPServerEditorModeId = TEXT("EM_UEMCPServerEditorMode");

FString UUEMCPServerEditorMode::SimpleToolName = TEXT("UEMCPServer_ActorInfoTool");
FString UUEMCPServerEditorMode::InteractiveToolName = TEXT("UEMCPServer_MeasureDistanceTool");


UUEMCPServerEditorMode::UUEMCPServerEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	// appearance and icon in the editing mode ribbon can be customized here
	Info = FEditorModeInfo(UUEMCPServerEditorMode::EM_UEMCPServerEditorModeId,
		LOCTEXT("ModeName", "UE MCP Server"),
		FSlateIcon(),
		true);
}


UUEMCPServerEditorMode::~UUEMCPServerEditorMode()
{
}


void UUEMCPServerEditorMode::ActorSelectionChangeNotify()
{
}

void UUEMCPServerEditorMode::Enter()
{
	UEdMode::Enter();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// AddYourTool Step 2 - register the ToolBuilders for your Tools here.
	// The string name you pass to the ToolManager is used to select/activate your ToolBuilder later.
	//////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////// 
	const FUEMCPServerEditorModeCommands& SampleToolCommands = FUEMCPServerEditorModeCommands::Get();

	RegisterTool(SampleToolCommands.SimpleTool, SimpleToolName, NewObject<UUEMCPServerSimpleToolBuilder>(this));
	RegisterTool(SampleToolCommands.InteractiveTool, InteractiveToolName, NewObject<UUEMCPServerInteractiveToolBuilder>(this));

	// active tool type is not relevant here, we just set to default
	GetToolManager()->SelectActiveToolType(EToolSide::Left, SimpleToolName);
}

void UUEMCPServerEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FUEMCPServerEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UUEMCPServerEditorMode::GetModeCommands() const
{
	return FUEMCPServerEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
