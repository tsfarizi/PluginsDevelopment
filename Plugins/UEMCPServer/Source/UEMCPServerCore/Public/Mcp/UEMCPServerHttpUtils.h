#pragma once

#include "CoreMinimal.h"
#include "HttpServerRequest.h"

class FJsonObject;

class UEMCPSERVERCORE_API UEMCPServerHttpUtils
{
public:
	static FString RequestBodyToString(const FHttpServerRequest& Request);
	static FString PeerEndpointString(const TSharedPtr<FInternetAddr>& PeerAddress);
	static bool ContainsToken(const FString& Source, const FString& Token);
	static TSharedPtr<FJsonObject> ParseJsonObject(const FString& Body);
	static FString MakeLogContext(const TCHAR* Phase, const FString& Endpoint, const FGuid& SessionId, const FString& Method, const FString& Accept);
	static void AppendSseEvent(FString& Output, const FString& Message);
	static FString ExtractHeaderValue(const TMap<FString, TArray<FString>>& Headers, const FString& HeaderName);
	static bool TryParseSessionId(const FString& RawValue, FGuid& OutSessionId);
};
