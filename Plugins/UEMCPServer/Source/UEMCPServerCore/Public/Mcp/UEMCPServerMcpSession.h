#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

class FJsonObject;
class FJsonValue;
class IUEMCPServerLiveCodingProvider;

class FUEMCPServerMcpSession : public TSharedFromThis<FUEMCPServerMcpSession>
{
public:
	FUEMCPServerMcpSession(IUEMCPServerLiveCodingProvider& InLiveCodingManager, const FGuid& InClientId, FString InEndpoint);

	bool HandleMessage(const FString& Message, TArray<FString>& OutgoingMessages);
	void HandleClosed();

	const FGuid& GetClientId() const { return ClientId; }
	const FString& GetEndpoint() const { return Endpoint; }

private:
	void ProcessMessage(const FString& Message);
	void RespondInitialize(const TSharedPtr<FJsonValue>& IdValue, const TSharedPtr<FJsonObject>& Params);
	void RespondToolsList(const TSharedPtr<FJsonValue>& IdValue);
	void RespondToolsCall(const TSharedPtr<FJsonValue>& IdValue, const TSharedPtr<FJsonObject>& Params);
	void RespondPing(const TSharedPtr<FJsonValue>& IdValue);

	void HandleCompileTool(const TSharedPtr<FJsonValue>& IdValue);
	void HandleStatusTool(const TSharedPtr<FJsonValue>& IdValue);

	void SendToolResult(const TSharedPtr<FJsonValue>& IdValue, const FString& MessageText, const TSharedRef<FJsonObject>& Structured, bool bIsError);
	void SendResponse(const TSharedPtr<FJsonValue>& IdValue, const TSharedRef<FJsonObject>& ResultObject);
	void SendError(const TSharedPtr<FJsonValue>& IdValue, int32 Code, const FString& ErrorMessage, const TSharedPtr<FJsonObject>& Data = nullptr);
	void SendParseError();
	void SendJson(const TSharedRef<FJsonObject>& Object);
	void WriteIdField(const TSharedPtr<FJsonValue>& IdValue, const TSharedRef<FJsonObject>& Target) const;

	TArray<TSharedPtr<FJsonValue>> MakeTextContentArray(const FString& MessageText) const;
	TSharedRef<FJsonObject> BuildLiveCodingStatus(FString& OutMessage) const;

private:
	IUEMCPServerLiveCodingProvider& LiveCodingManager;
	FGuid ClientId;
	FString Endpoint;
	bool bInitialized;
	TArray<FString> PendingMessages;
	FCriticalSection SessionMutex;
};
