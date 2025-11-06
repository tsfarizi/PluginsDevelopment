#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "Misc/Guid.h"

class FSlateAgentBridgeLiveCodingManager;
class FSlateAgentBridgeMcpSession;
class IHttpRouter;

class FSlateAgentBridgeMcpServer
{
public:
	FSlateAgentBridgeMcpServer(FSlateAgentBridgeLiveCodingManager& InLiveCodingManager, uint32 InPort, const FString& InBindAddress);
	~FSlateAgentBridgeMcpServer();

	bool Start();
	void Stop();

private:
	bool HandlePostRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleGetRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	TSharedPtr<FSlateAgentBridgeMcpSession> FindSessionById(const FGuid& ClientId);
	TSharedPtr<FSlateAgentBridgeMcpSession> CreateSession(const FString& Endpoint, FGuid& OutSessionId);
	TSharedPtr<FSlateAgentBridgeMcpSession> FindSessionForEndpoint(const FString& Endpoint, FGuid& OutSessionId);
	TSharedPtr<FSlateAgentBridgeMcpSession> FindDefaultSession(FGuid& OutSessionId);
	void AssociateEndpointWithSession(const FString& Endpoint, const FGuid& SessionId);
	bool ValidateProtocolVersion(const FString& ProtocolVersionHeader) const;
	void SetSessionOverrideConfig() const;
	static bool TryParseSessionId(const FString& RawValue, FGuid& OutSessionId);
	static FString ExtractHeaderValue(const TMap<FString, TArray<FString>>& Headers, const FString& HeaderName);

	FSlateAgentBridgeLiveCodingManager& LiveCodingManager;
	uint32 Port;
	FString BindAddress;
	FString EndpointPath;

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle PostRouteHandle;
	FHttpRouteHandle GetRouteHandle;
	bool bListenersStarted;

	FCriticalSection SessionMutex;
	TMap<FGuid, TSharedPtr<FSlateAgentBridgeMcpSession>> Sessions;
	TMap<FString, FGuid> EndpointToSession;
};
