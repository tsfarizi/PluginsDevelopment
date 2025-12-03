#include "Mcp/UEMCPServerMcpSchema.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UEMCPServer::Mcp
{
	static const TCHAR* CompileToolName = TEXT("liveCoding_compile");
	static const TCHAR* StatusToolName = TEXT("liveCoding_status");
}

TSharedRef<FJsonObject> UEMCPServerMcpSchema::BuildToolInputSchema(bool bIncludeWaitFlag)
{
	TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("type"), TEXT("object"));

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	if (bIncludeWaitFlag)
	{
		TSharedRef<FJsonObject> WaitProp = MakeShared<FJsonObject>();
		WaitProp->SetStringField(TEXT("type"), TEXT("boolean"));
		WaitProp->SetStringField(TEXT("description"), TEXT("Reserved for future use. When true, the server will wait for the compile to finish before responding."));
		Properties->SetObjectField(TEXT("waitForCompletion"), WaitProp);
	}
	Schema->SetObjectField(TEXT("properties"), Properties);
	Schema->SetBoolField(TEXT("additionalProperties"), false);

	return Schema;
}

TSharedRef<FJsonObject> UEMCPServerMcpSchema::BuildLiveCodingOutputSchema()
{
	TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("type"), TEXT("object"));

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

	auto MakeStringProperty = [](const FString& Description)
	{
		TSharedRef<FJsonObject> Prop = MakeShared<FJsonObject>();
		Prop->SetStringField(TEXT("type"), TEXT("string"));
		if (!Description.IsEmpty())
		{
			Prop->SetStringField(TEXT("description"), Description);
		}
		return Prop;
	};

	auto MakeBooleanProperty = [](const FString& Description)
	{
		TSharedRef<FJsonObject> Prop = MakeShared<FJsonObject>();
		Prop->SetStringField(TEXT("type"), TEXT("boolean"));
		if (!Description.IsEmpty())
		{
			Prop->SetStringField(TEXT("description"), Description);
		}
		return Prop;
	};

	Properties->SetObjectField(TEXT("status"), MakeStringProperty(TEXT("High-level status of the call (ok, error, etc.).")));
	Properties->SetObjectField(TEXT("message"), MakeStringProperty(TEXT("Human-readable summary of the snapshot.")));
	Properties->SetObjectField(TEXT("compileResult"), MakeStringProperty(TEXT("Final Live Coding compile result.")));
	Properties->SetObjectField(TEXT("compileInProgress"), MakeBooleanProperty(TEXT("True if a compile is currently running.")));
	Properties->SetObjectField(TEXT("hasPreviousResult"), MakeBooleanProperty(TEXT("True if a previous compile result is available.")));
	Properties->SetObjectField(TEXT("compileStarted"), MakeBooleanProperty(TEXT("True if the request queued a new compile.")));
	Properties->SetObjectField(TEXT("timestampUtc"), MakeStringProperty(TEXT("UTC timestamp of the snapshot when available.")));

	TSharedRef<FJsonObject> LogItems = MakeShared<FJsonObject>();
	LogItems->SetStringField(TEXT("type"), TEXT("object"));
	TSharedPtr<FJsonObject> LogProperties = MakeShared<FJsonObject>();
	LogProperties->SetObjectField(TEXT("timeUtc"), MakeStringProperty(TEXT("Timestamp of the log entry in UTC.")));
	LogProperties->SetObjectField(TEXT("category"), MakeStringProperty(TEXT("Log category.")));
	LogProperties->SetObjectField(TEXT("verbosity"), MakeStringProperty(TEXT("Verbosity string.")));
	LogProperties->SetObjectField(TEXT("message"), MakeStringProperty(TEXT("Log message text.")));
	LogItems->SetObjectField(TEXT("properties"), LogProperties);
	LogItems->SetBoolField(TEXT("additionalProperties"), false);

	TSharedRef<FJsonObject> LogArray = MakeShared<FJsonObject>();
	LogArray->SetStringField(TEXT("type"), TEXT("array"));
	LogArray->SetObjectField(TEXT("items"), LogItems);
	Properties->SetObjectField(TEXT("log"), LogArray);

	Schema->SetObjectField(TEXT("properties"), Properties);

	TArray<TSharedPtr<FJsonValue>> Required;
	Required.Add(MakeShared<FJsonValueString>(TEXT("status")));
	Required.Add(MakeShared<FJsonValueString>(TEXT("message")));
	Required.Add(MakeShared<FJsonValueString>(TEXT("compileResult")));
	Required.Add(MakeShared<FJsonValueString>(TEXT("compileInProgress")));
	Schema->SetArrayField(TEXT("required"), Required);
	Schema->SetBoolField(TEXT("additionalProperties"), true);

	return Schema;
}

void UEMCPServerMcpSchema::PopulateToolsList(TArray<TSharedPtr<FJsonValue>>& OutTools)
{
	TSharedRef<FJsonObject> CompileTool = MakeShared<FJsonObject>();
	CompileTool->SetStringField(TEXT("name"), UEMCPServer::Mcp::CompileToolName);
	CompileTool->SetStringField(TEXT("description"), TEXT("Trigger a UE Live Coding compile and return the latest compile snapshot."));
	CompileTool->SetObjectField(TEXT("inputSchema"), BuildToolInputSchema(true));
	CompileTool->SetObjectField(TEXT("outputSchema"), BuildLiveCodingOutputSchema());
	TSharedPtr<FJsonObject> CompileAnnotations = MakeShared<FJsonObject>();
	CompileAnnotations->SetBoolField(TEXT("destructiveHint"), false);
	CompileAnnotations->SetBoolField(TEXT("readOnlyHint"), false);
	CompileAnnotations->SetStringField(TEXT("title"), TEXT("Trigger Live Coding Compile"));
	CompileTool->SetObjectField(TEXT("annotations"), CompileAnnotations);
	OutTools.Add(MakeShared<FJsonValueObject>(CompileTool));

	TSharedRef<FJsonObject> StatusTool = MakeShared<FJsonObject>();
	StatusTool->SetStringField(TEXT("name"), UEMCPServer::Mcp::StatusToolName);
	StatusTool->SetStringField(TEXT("description"), TEXT("Return the most recent Live Coding compile snapshot without starting a new compile."));
	StatusTool->SetObjectField(TEXT("inputSchema"), BuildToolInputSchema(false));
	StatusTool->SetObjectField(TEXT("outputSchema"), BuildLiveCodingOutputSchema());
	TSharedPtr<FJsonObject> StatusAnnotations = MakeShared<FJsonObject>();
	StatusAnnotations->SetBoolField(TEXT("destructiveHint"), false);
	StatusAnnotations->SetBoolField(TEXT("readOnlyHint"), true);
	StatusAnnotations->SetStringField(TEXT("title"), TEXT("Get Live Coding Status"));
	StatusTool->SetObjectField(TEXT("annotations"), StatusAnnotations);
	OutTools.Add(MakeShared<FJsonValueObject>(StatusTool));
}
