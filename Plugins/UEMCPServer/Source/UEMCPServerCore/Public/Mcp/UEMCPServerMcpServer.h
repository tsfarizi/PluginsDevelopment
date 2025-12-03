#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "Misc/Guid.h"

class IUEMCPServerLiveCodingProvider;
class FUEMCPServerMcpSession;
class IHttpRouter;

class IHttpRouter;

class UEMCPSERVERCORE_API FUEMCPServerMcpServer
{
public:
	FUEMCPServerMcpServer(IUEMCPServerLiveCodingProvider& InLiveCodingManager, uint32 InPort, const FString& InBindAddress);
	~FUEMCPServerMcpServer();

	bool Start();
	void Stop();

private:
	bool HandlePostRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleGetRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	TSharedPtr<FUEMCPServerMcpSession> FindSessionById(const FGuid& ClientId);
	TSharedPtr<FUEMCPServerMcpSession> CreateSession(const FString& Endpoint, FGuid& OutSessionId);
	TSharedPtr<FUEMCPServerMcpSession> FindSessionForEndpoint(const FString& Endpoint, FGuid& OutSessionId);
	TSharedPtr<FUEMCPServerMcpSession> FindDefaultSession(FGuid& OutSessionId);
	void AssociateEndpointWithSession(const FString& Endpoint, const FGuid& SessionId);
	bool ValidateProtocolVersion(const FString& ProtocolVersionHeader) const;
	void SetSessionOverrideConfig() const;


	IUEMCPServerLiveCodingProvider& LiveCodingManager;
	uint32 Port;
	FString BindAddress;
	FString EndpointPath;

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle PostRouteHandle;
	FHttpRouteHandle GetRouteHandle;
	bool bListenersStarted;

	FCriticalSection SessionMutex;
	TMap<FGuid, TSharedPtr<FUEMCPServerMcpSession>> Sessions;
	TMap<FString, FGuid> EndpointToSession;
};
