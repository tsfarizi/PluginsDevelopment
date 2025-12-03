#include "Mcp/UEMCPServerMcpSession.h"
#include "Mcp/UEMCPServerMcpSchema.h"
#include "IUEMCPServerLiveCodingProvider.h"
#include "UEMCPServerLiveCodingTypes.h"
#include "UEMCPServerLog.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Containers/StringConv.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ILiveCodingModule.h"

namespace UEMCPServer::Mcp
{
	static const TCHAR* InitializeMethod = TEXT("initialize");
	static const TCHAR* ToolsListMethod = TEXT("tools/list");
	static const TCHAR* ToolsCallMethod = TEXT("tools/call");
	static const TCHAR* PingMethod = TEXT("ping");
	static const TCHAR* InitializedNotification = TEXT("notifications/initialized");

	static const TCHAR* CompileToolName = TEXT("liveCoding_compile");
	static const TCHAR* StatusToolName = TEXT("liveCoding_status");

	static const TCHAR* ProtocolVersion = TEXT("2025-06-18");
}

namespace
{
	constexpr int32 JsonRpcParseError = -32700;
	constexpr int32 JsonRpcInvalidRequest = -32600;
	constexpr int32 JsonRpcMethodNotFound = -32601;
	constexpr int32 JsonRpcInvalidParams = -32602;
	constexpr int32 JsonRpcInternalError = -32603;
	constexpr int32 JsonRpcServerError = -32000;
}

FUEMCPServerMcpSession::FUEMCPServerMcpSession(IUEMCPServerLiveCodingProvider& InLiveCodingManager, const FGuid& InClientId, FString InEndpoint)
	: LiveCodingManager(InLiveCodingManager)
	, ClientId(InClientId)
	, Endpoint(MoveTemp(InEndpoint))
	, bInitialized(false)
{
}

bool FUEMCPServerMcpSession::HandleMessage(const FString& Message, TArray<FString>& OutgoingMessages)
{
	FScopeLock Guard(&SessionMutex);
	PendingMessages.Reset();
	ProcessMessage(Message);
	OutgoingMessages = PendingMessages;
	PendingMessages.Reset();
	return true;
}

void FUEMCPServerMcpSession::HandleClosed()
{
	bInitialized = false;
	PendingMessages.Reset();
}

void FUEMCPServerMcpSession::ProcessMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		const FString ClientIdString = ClientId.ToString();
		UE_LOG(LogUEMCPServer, Warning, TEXT("Received invalid JSON from MCP client %s"), *ClientIdString);
		SendParseError();
		return;
	}

	TSharedPtr<FJsonValue> IdValue = Object->TryGetField(TEXT("id"));

	FString JsonRpcVersion;
	if (!Object->TryGetStringField(TEXT("jsonrpc"), JsonRpcVersion) || JsonRpcVersion != TEXT("2.0"))
	{
		const FString ClientIdString = ClientId.ToString();
		UE_LOG(LogUEMCPServer, Warning, TEXT("Received non JSON-RPC 2.0 message from MCP client %s"), *ClientIdString);
		SendError(IdValue, JsonRpcInvalidRequest, TEXT("Only JSON-RPC 2.0 is supported."));
		return;
	}

	FString Method;
	if (!Object->TryGetStringField(TEXT("method"), Method))
	{
		// Response from client; nothing to do.
		return;
	}

	TSharedPtr<FJsonObject> ParamsObject;
	if (Object->HasTypedField<EJson::Object>(TEXT("params")))
	{
		ParamsObject = Object->GetObjectField(TEXT("params"));
	}

	if (Method == UEMCPServer::Mcp::InitializeMethod)
	{
		RespondInitialize(IdValue, ParamsObject);
		return;
	}

	if (!IdValue.IsValid())
	{
		if (Method == UEMCPServer::Mcp::InitializedNotification)
		{
			bInitialized = true;
			const FString ClientIdString = ClientId.ToString();
			UE_LOG(LogUEMCPServer, Verbose, TEXT("MCP client %s acknowledged initialization."), *ClientIdString);
		}
		return;
	}

	if (!bInitialized)
	{
		SendError(IdValue, JsonRpcServerError, TEXT("Client must complete initialize before issuing requests."));
		return;
	}

	if (Method == UEMCPServer::Mcp::ToolsListMethod)
	{
		RespondToolsList(IdValue);
	}
	else if (Method == UEMCPServer::Mcp::ToolsCallMethod)
	{
		RespondToolsCall(IdValue, ParamsObject);
	}
	else if (Method == UEMCPServer::Mcp::PingMethod)
	{
		RespondPing(IdValue);
	}
	else
	{
		SendError(IdValue, JsonRpcMethodNotFound, FString::Printf(TEXT("Method '%s' is not implemented."), *Method));
	}
}

void FUEMCPServerMcpSession::RespondInitialize(const TSharedPtr<FJsonValue>& IdValue, const TSharedPtr<FJsonObject>& Params)
{
	FString RequestedProtocol = UEMCPServer::Mcp::ProtocolVersion;
	if (Params.IsValid())
	{
		FString ProtocolField;
		if (Params->TryGetStringField(TEXT("protocolVersion"), ProtocolField) && !ProtocolField.IsEmpty())
		{
			RequestedProtocol = ProtocolField;
		}
	}

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), RequestedProtocol);

	TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), TEXT("UE MCP Server"));
	ServerInfo->SetStringField(TEXT("version"), TEXT("1.0.0"));
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> ToolsCaps = MakeShared<FJsonObject>();
	ToolsCaps->SetBoolField(TEXT("listChanged"), false);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCaps);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	Result->SetStringField(TEXT("instructions"), TEXT("Use tools/list to discover the available Live Coding tools. Call liveCoding_compile to trigger a compile or liveCoding_status for the latest snapshot."));

	SendResponse(IdValue, Result);

	bInitialized = true;

	const FString ClientIdString = ClientId.ToString();
	UE_LOG(LogUEMCPServer, Verbose, TEXT("MCP client %s initialized (%s)."), *ClientIdString, Endpoint.IsEmpty() ? TEXT("unknown") : *Endpoint);
}

void FUEMCPServerMcpSession::RespondToolsList(const TSharedPtr<FJsonValue>& IdValue)
{
	TArray<TSharedPtr<FJsonValue>> Tools;
	UEMCPServerMcpSchema::PopulateToolsList(Tools);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), Tools);

	SendResponse(IdValue, Result);
}

void FUEMCPServerMcpSession::RespondToolsCall(const TSharedPtr<FJsonValue>& IdValue, const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		SendError(IdValue, JsonRpcInvalidParams, TEXT("Missing params object for tools/call."));
		return;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName) || ToolName.IsEmpty())
	{
		SendError(IdValue, JsonRpcInvalidParams, TEXT("Missing tool name for tools/call."));
		return;
	}

	if (ToolName == UEMCPServer::Mcp::CompileToolName)
	{
		HandleCompileTool(IdValue);
	}
	else if (ToolName == UEMCPServer::Mcp::StatusToolName)
	{
		HandleStatusTool(IdValue);
	}
	else
	{
		SendError(IdValue, JsonRpcMethodNotFound, FString::Printf(TEXT("Unknown tool '%s'."), *ToolName));
	}
}

void FUEMCPServerMcpSession::RespondPing(const TSharedPtr<FJsonValue>& IdValue)
{
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	SendResponse(IdValue, Result);
}

void FUEMCPServerMcpSession::HandleCompileTool(const TSharedPtr<FJsonValue>& IdValue)
{
	FString ErrorMessage;
	if (!LiveCodingManager.TryBeginCompile(ErrorMessage))
	{
		TSharedRef<FJsonObject> Structured = MakeShared<FJsonObject>();
		Structured->SetStringField(TEXT("status"), TEXT("error"));
		Structured->SetStringField(TEXT("message"), ErrorMessage);
		Structured->SetBoolField(TEXT("compileInProgress"), true);
		Structured->SetBoolField(TEXT("compileStarted"), false);
		SendToolResult(IdValue, ErrorMessage, Structured, true);
		return;
	}

	FString StatusMessage;
	TSharedRef<FJsonObject> Structured = BuildLiveCodingStatus(StatusMessage);
	StatusMessage = TEXT("Compile queued. Poll liveCoding.status for updates.");
	Structured->SetStringField(TEXT("status"), TEXT("ok"));
	Structured->SetStringField(TEXT("message"), StatusMessage);
	Structured->SetBoolField(TEXT("compileStarted"), true);

	SendToolResult(IdValue, StatusMessage, Structured, false);

	FUEMCPServerMcpSession* SessionPtr = this;
	AsyncTask(ENamedThreads::GameThread, [SessionPtr]()
	{
		if (SessionPtr)
		{
			SessionPtr->LiveCodingManager.ExecuteCompileOnGameThread();
		}
	});

	const FString ClientIdString = ClientId.ToString();
	UE_LOG(LogUEMCPServer, Verbose, TEXT("MCP client %s queued Live Coding compile."), *ClientIdString);
}

void FUEMCPServerMcpSession::HandleStatusTool(const TSharedPtr<FJsonValue>& IdValue)
{
	FString StatusMessage;
	TSharedRef<FJsonObject> Structured = BuildLiveCodingStatus(StatusMessage);
	SendToolResult(IdValue, StatusMessage, Structured, false);

	const FString ClientIdString = ClientId.ToString();
	UE_LOG(LogUEMCPServer, Verbose, TEXT("MCP client %s requested Live Coding status."), *ClientIdString);
}

void FUEMCPServerMcpSession::SendToolResult(const TSharedPtr<FJsonValue>& IdValue, const FString& MessageText, const TSharedRef<FJsonObject>& Structured, bool bIsError)
{
	TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetArrayField(TEXT("content"), MakeTextContentArray(MessageText.IsEmpty() ? TEXT(" ") : MessageText));
	ResultObject->SetObjectField(TEXT("structuredContent"), Structured);
	if (bIsError)
	{
		ResultObject->SetBoolField(TEXT("isError"), true);
	}

	SendResponse(IdValue, ResultObject);
}

void FUEMCPServerMcpSession::SendResponse(const TSharedPtr<FJsonValue>& IdValue, const TSharedRef<FJsonObject>& ResultObject)
{
	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	WriteIdField(IdValue, Response);
	Response->SetObjectField(TEXT("result"), ResultObject);

	SendJson(Response);
}

void FUEMCPServerMcpSession::SendError(const TSharedPtr<FJsonValue>& IdValue, int32 Code, const FString& ErrorMessage, const TSharedPtr<FJsonObject>& Data)
{
	TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
	ErrorObject->SetNumberField(TEXT("code"), Code);
	ErrorObject->SetStringField(TEXT("message"), ErrorMessage);
	if (Data.IsValid())
	{
		ErrorObject->SetObjectField(TEXT("data"), Data.ToSharedRef());
	}

	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	WriteIdField(IdValue, Response);
	Response->SetObjectField(TEXT("error"), ErrorObject);

	SendJson(Response);
}

void FUEMCPServerMcpSession::SendParseError()
{
	SendError(TSharedPtr<FJsonValue>(), JsonRpcParseError, TEXT("Failed to parse JSON-RPC message."));
}

void FUEMCPServerMcpSession::SendJson(const TSharedRef<FJsonObject>& Object)
{
	FString Payload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(Object, Writer);

	PendingMessages.Add(MoveTemp(Payload));
}

void FUEMCPServerMcpSession::WriteIdField(const TSharedPtr<FJsonValue>& IdValue, const TSharedRef<FJsonObject>& Target) const
{
	if (!IdValue.IsValid())
	{
		Target->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
		return;
	}

	switch (IdValue->Type)
	{
	case EJson::String:
		Target->SetStringField(TEXT("id"), IdValue->AsString());
		break;
	case EJson::Number:
		Target->SetNumberField(TEXT("id"), IdValue->AsNumber());
		break;
	case EJson::Null:
		Target->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
		break;
	case EJson::Boolean:
		Target->SetBoolField(TEXT("id"), IdValue->AsBool());
		break;
	case EJson::Array:
	case EJson::Object:
		Target->SetField(TEXT("id"), IdValue);
		break;
	}
}

TArray<TSharedPtr<FJsonValue>> FUEMCPServerMcpSession::MakeTextContentArray(const FString& MessageText) const
{
	TArray<TSharedPtr<FJsonValue>> Content;
	TSharedRef<FJsonObject> ContentObject = MakeShared<FJsonObject>();
	ContentObject->SetStringField(TEXT("type"), TEXT("text"));
	ContentObject->SetStringField(TEXT("text"), MessageText);
	Content.Add(MakeShared<FJsonValueObject>(ContentObject));
	return Content;
}

TSharedRef<FJsonObject> FUEMCPServerMcpSession::BuildLiveCodingStatus(FString& OutMessage) const
{
	TArray<FUEMCPServerLogEntry> LogSnapshot;
	FDateTime SnapshotTimestamp;
	ELiveCodingCompileResult SnapshotResult = ELiveCodingCompileResult::NotStarted;
	bool bHasSnapshotResult = false;
	FString SnapshotError;
	bool bInProgress = false;

	LiveCodingManager.GetLastCompileSnapshot(LogSnapshot, SnapshotTimestamp, SnapshotResult, bHasSnapshotResult, SnapshotError, bInProgress);

	TSharedRef<FJsonObject> Status = MakeShared<FJsonObject>();
	const FString ResultString = UEMCPServer::CompileResultToString(SnapshotResult);

	Status->SetStringField(TEXT("status"), SnapshotError.IsEmpty() ? TEXT("ok") : TEXT("error"));
	Status->SetStringField(TEXT("compileResult"), ResultString);
	Status->SetBoolField(TEXT("compileInProgress"), bInProgress);
	Status->SetBoolField(TEXT("hasPreviousResult"), bHasSnapshotResult);
	Status->SetBoolField(TEXT("compileStarted"), false);

	if (SnapshotTimestamp.GetTicks() > 0)
	{
		Status->SetStringField(TEXT("timestampUtc"), SnapshotTimestamp.ToIso8601());
	}

	if (!SnapshotError.IsEmpty())
	{
		OutMessage = SnapshotError;
	}
	else if (bInProgress)
	{
		OutMessage = TEXT("Compile in progress.");
	}
	else if (!bHasSnapshotResult)
	{
		OutMessage = TEXT("No compile has been executed yet.");
	}
	else
	{
		OutMessage = FString::Printf(TEXT("Last compile result: %s."), *ResultString);
	}

	Status->SetStringField(TEXT("message"), OutMessage);

	TArray<TSharedPtr<FJsonValue>> LogArray;
	LogArray.Reserve(LogSnapshot.Num());
	for (const FUEMCPServerLogEntry& Entry : LogSnapshot)
	{
		TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("timeUtc"), Entry.Timestamp.ToIso8601());
		EntryObject->SetStringField(TEXT("category"), Entry.Category);
		EntryObject->SetStringField(TEXT("verbosity"), Entry.Verbosity);
		EntryObject->SetStringField(TEXT("message"), Entry.Message);
		LogArray.Add(MakeShared<FJsonValueObject>(EntryObject));
	}
	Status->SetArrayField(TEXT("log"), LogArray);

	return Status;
}
