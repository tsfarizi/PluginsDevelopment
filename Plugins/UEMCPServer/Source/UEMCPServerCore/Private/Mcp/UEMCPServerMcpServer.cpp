#include "Mcp/UEMCPServerMcpServer.h"

#include "Mcp/UEMCPServerMcpSession.h"
#include "IUEMCPServerLiveCodingProvider.h"
#include "UEMCPServerLog.h"

#include "HttpPath.h"
#include "HttpServerConstants.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Containers/StringConv.h"
#include "Templates/UniquePtr.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UEMCPServer
{
	static constexpr const TCHAR* DefaultMcpEndpointPath = TEXT("/mcp");
	static constexpr const TCHAR* ProtocolVersionHeader = TEXT("MCP-Protocol-Version");
	static constexpr const TCHAR* SessionIdHeader = TEXT("Mcp-Session-Id");
	static constexpr const TCHAR* AcceptHeader = TEXT("accept");
	static constexpr const TCHAR* ContentTypeJson = TEXT("application/json");
	static constexpr const TCHAR* ContentTypeEventStream = TEXT("text/event-stream");
	static constexpr const TCHAR* ContentTypeEventStreamResponse = TEXT("text/event-stream");
	static constexpr const TCHAR* CacheControlHeader = TEXT("cache-control");
	static constexpr const TCHAR* NoStoreValue = TEXT("no-store");
	static constexpr const TCHAR* HttpListenersSection = TEXT("HTTPServer.Listeners");
	static constexpr const TCHAR* ListenerOverridesKey = TEXT("ListenerOverrides");
	static constexpr const TCHAR* ProtocolVersionValue = TEXT("2025-06-18");
}

#include "Mcp/UEMCPServerHttpUtils.h"

namespace
{

}

FUEMCPServerMcpServer::FUEMCPServerMcpServer(IUEMCPServerLiveCodingProvider& InLiveCodingManager, uint32 InPort, const FString& InBindAddress)
	: LiveCodingManager(InLiveCodingManager)
	, Port(InPort)
	, BindAddress(InBindAddress)
	, EndpointPath(UEMCPServer::DefaultMcpEndpointPath)
	, bListenersStarted(false)
{
}

FUEMCPServerMcpServer::~FUEMCPServerMcpServer()
{
	Stop();
}

bool FUEMCPServerMcpServer::Start()
{
	if (Router.IsValid())
	{
		return true;
	}

	SetSessionOverrideConfig();

	FHttpServerModule& HttpModule = FHttpServerModule::Get();
	Router = HttpModule.GetHttpRouter(Port, /*bFailOnBindFailure=*/true);
	if (!Router.IsValid())
	{
		UE_LOG(LogUEMCPServer, Error, TEXT("Unable to start MCP HTTP server on %s:%u"), BindAddress.IsEmpty() ? TEXT("0.0.0.0") : *BindAddress, Port);
		return false;
	}

	const FHttpPath EndpointPathObject(EndpointPath);

	PostRouteHandle = Router->BindRoute(
		EndpointPathObject,
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUEMCPServerMcpServer::HandlePostRequest));

	if (!PostRouteHandle.IsValid())
	{
		UE_LOG(LogUEMCPServer, Error, TEXT("Failed to bind MCP POST handler at %s"), *EndpointPathObject.GetPath());
		Router.Reset();
		return false;
	}

	GetRouteHandle = Router->BindRoute(
		EndpointPathObject,
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUEMCPServerMcpServer::HandleGetRequest));

	if (!GetRouteHandle.IsValid())
	{
		UE_LOG(LogUEMCPServer, Error, TEXT("Failed to bind MCP GET handler at %s"), *EndpointPathObject.GetPath());
		Router->UnbindRoute(PostRouteHandle);
		Router.Reset();
		return false;
	}

	if (!bListenersStarted)
	{
		HttpModule.StartAllListeners();
		bListenersStarted = true;
	}

	UE_LOG(LogUEMCPServer, Display, TEXT("UEMCPServer MCP server listening on http://%s:%u%s"),
		BindAddress.IsEmpty() ? TEXT("127.0.0.1") : *BindAddress,
		Port,
		*EndpointPathObject.GetPath());

	return true;
}

void FUEMCPServerMcpServer::Stop()
{
	if (Router.IsValid())
	{
		if (PostRouteHandle.IsValid())
		{
			Router->UnbindRoute(PostRouteHandle);
			PostRouteHandle = FHttpRouteHandle();
		}

		if (GetRouteHandle.IsValid())
		{
			Router->UnbindRoute(GetRouteHandle);
			GetRouteHandle = FHttpRouteHandle();
		}
	}

	FHttpServerModule& HttpModule = FHttpServerModule::Get();
	if (bListenersStarted)
	{
		HttpModule.StopAllListeners();
		bListenersStarted = false;
	}

	Router.Reset();

	{
		FScopeLock Guard(&SessionMutex);
		for (auto& SessionPair : Sessions)
		{
			if (SessionPair.Value.IsValid())
			{
				SessionPair.Value->HandleClosed();
			}
		}
		Sessions.Empty();
		EndpointToSession.Empty();
	}
}

bool FUEMCPServerMcpServer::HandlePostRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString Body = UEMCPServerHttpUtils::RequestBodyToString(Request);
	if (Body.IsEmpty())
	{
		UE_LOG(LogUEMCPServer, Warning, TEXT("%s -> rejecting: empty body"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), UEMCPServerHttpUtils::PeerEndpointString(Request.PeerAddress), FGuid(), FString(), UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::AcceptHeader)));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("empty_body"), TEXT("Request body is required.")));
		return true;
	}

	const FString ProtocolVersionHeaderValue = UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::ProtocolVersionHeader);
	if (!ValidateProtocolVersion(ProtocolVersionHeaderValue))
	{
		UE_LOG(LogUEMCPServer, Warning, TEXT("%s -> rejecting: unsupported protocol %s"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), UEMCPServerHttpUtils::PeerEndpointString(Request.PeerAddress), FGuid(), FString(), UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::AcceptHeader)),
			*ProtocolVersionHeaderValue);
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_protocol_version"), TEXT("Unsupported MCP protocol version.")));
		return true;
	}

	TSharedPtr<FJsonObject> JsonObject = UEMCPServerHttpUtils::ParseJsonObject(Body);
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogUEMCPServer, Warning, TEXT("%s -> rejecting: invalid JSON"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), UEMCPServerHttpUtils::PeerEndpointString(Request.PeerAddress), FGuid(), FString(), UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::AcceptHeader)));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json"), TEXT("Failed to parse JSON-RPC payload.")));
		return true;
	}

	const FString AcceptHeaderValue = UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::AcceptHeader);
	const bool bClientAcceptsJson = AcceptHeaderValue.IsEmpty() || UEMCPServerHttpUtils::ContainsToken(AcceptHeaderValue, UEMCPServer::ContentTypeJson);
	const bool bClientAcceptsSse = UEMCPServerHttpUtils::ContainsToken(AcceptHeaderValue, UEMCPServer::ContentTypeEventStream);
	if (!bClientAcceptsJson && !bClientAcceptsSse)
	{
		UE_LOG(LogUEMCPServer, Warning, TEXT("%s -> rejecting: unsupported Accept"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), UEMCPServerHttpUtils::PeerEndpointString(Request.PeerAddress), FGuid(), FString(), AcceptHeaderValue));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NoneAcceptable, TEXT("unsupported_accept"), TEXT("Client must accept application/json or text/event-stream.")));
		return true;
	}

	const FString Endpoint = UEMCPServerHttpUtils::PeerEndpointString(Request.PeerAddress);

	FGuid SessionId;
	const FString SessionIdHeaderValue = UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::SessionIdHeader);
	const bool bHasSessionHeader = UEMCPServerHttpUtils::TryParseSessionId(SessionIdHeaderValue, SessionId);

	bool bIsInitializeRequest = false;
	FString Method;
	if (JsonObject->TryGetStringField(TEXT("method"), Method))
	{
		bIsInitializeRequest = Method.Equals(TEXT("initialize"), ESearchCase::CaseSensitive);
	}

	UE_LOG(LogUEMCPServer, Verbose, TEXT("MCP POST %s from %s (Accept=%s, HasSessionHeader=%s)"),
		Method.IsEmpty() ? TEXT("<response>") : *Method,
		Endpoint.IsEmpty() ? TEXT("unknown") : *Endpoint,
		AcceptHeaderValue.IsEmpty() ? TEXT("<none>") : *AcceptHeaderValue,
		bHasSessionHeader ? TEXT("true") : TEXT("false"));

	TSharedPtr<FUEMCPServerMcpSession> Session;
	if (bHasSessionHeader)
	{
		Session = FindSessionById(SessionId);
		if (!Session.IsValid())
		{
			OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("unknown_session"), TEXT("MCP session not found.")));
			return true;
		}
		AssociateEndpointWithSession(Endpoint, SessionId);
		UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> using header session"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
	}
	else if (bIsInitializeRequest)
	{
		Session = FindSessionForEndpoint(Endpoint, SessionId);
		if (Session.IsValid())
		{
			UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> initialize reuse endpoint session"),
				*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		}
		if (!Session.IsValid())
		{
			Session = FindDefaultSession(SessionId);
			if (Session.IsValid())
			{
				AssociateEndpointWithSession(Endpoint, SessionId);
				UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> initialize reuse default session"),
					*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
			}
		}

		if (!Session.IsValid())
		{
			Session = CreateSession(Endpoint, SessionId);
		}
	}
	else
	{
		Session = FindSessionForEndpoint(Endpoint, SessionId);
		if (Session.IsValid())
		{
			UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> reuse endpoint session"),
				*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		}
		if (!Session.IsValid())
		{
			Session = FindDefaultSession(SessionId);
			if (Session.IsValid())
			{
				AssociateEndpointWithSession(Endpoint, SessionId);
				UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> request reuse default session"),
					*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
			}
		}

		if (!Session.IsValid())
		{
			UE_LOG(LogUEMCPServer, Warning, TEXT("%s -> rejecting: session missing"),
				*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
			OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("missing_session"), TEXT("Mcp-Session-Id header is required.")));
			return true;
		}
	}

	TArray<FString> PendingMessages;
	if (!Session->HandleMessage(Body, PendingMessages))
	{
		UE_LOG(LogUEMCPServer, Warning, TEXT("%s -> session processing error"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::ServerError, TEXT("session_error"), TEXT("Failed to process MCP message.")));
		return true;
	}

	if (PendingMessages.IsEmpty())
	{
		TUniquePtr<FHttpServerResponse> AcceptedResponse = MakeUnique<FHttpServerResponse>();
		AcceptedResponse->Code = EHttpServerResponseCodes::Accepted;
		AcceptedResponse->Headers.Add(UEMCPServer::CacheControlHeader, { UEMCPServer::NoStoreValue });
		if (SessionId.IsValid())
		{
			AcceptedResponse->Headers.Add(UEMCPServer::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
		}
		AcceptedResponse->Headers.Add(UEMCPServer::ProtocolVersionHeader, { UEMCPServer::ProtocolVersionValue });
		UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> returning 202 Accepted"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(MoveTemp(AcceptedResponse));
		return true;
	}

	if (PendingMessages.Num() == 1 && bClientAcceptsJson)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(PendingMessages[0], UEMCPServer::ContentTypeJson);
		Response->Headers.Add(UEMCPServer::CacheControlHeader, { UEMCPServer::NoStoreValue });
		if (SessionId.IsValid())
		{
			Response->Headers.Add(UEMCPServer::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
		}
		Response->Headers.Add(UEMCPServer::ProtocolVersionHeader, { UEMCPServer::ProtocolVersionValue });
		UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> returning JSON response"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(MoveTemp(Response));
		return true;
	}

	if (!bClientAcceptsSse)
	{
		UE_LOG(LogUEMCPServer, Warning, TEXT("%s -> rejecting: SSE required for multi-message response"),
			*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NoneAcceptable, TEXT("sse_required"), TEXT("Client must accept text/event-stream for multi-message responses.")));
		return true;
	}

	FString SsePayload;
	SsePayload.Reserve(PendingMessages.Num() * 64);
	for (const FString& Message : PendingMessages)
	{
		UEMCPServerHttpUtils::AppendSseEvent(SsePayload, Message);
	}

	TUniquePtr<FHttpServerResponse> SseResponse = FHttpServerResponse::Create(SsePayload, UEMCPServer::ContentTypeEventStreamResponse);
	SseResponse->Headers.Add(UEMCPServer::CacheControlHeader, { UEMCPServer::NoStoreValue });
	if (SessionId.IsValid())
	{
		SseResponse->Headers.Add(UEMCPServer::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
	}
	SseResponse->Headers.Add(UEMCPServer::ProtocolVersionHeader, { UEMCPServer::ProtocolVersionValue });
	UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> returning SSE (%d message(s))"),
		*UEMCPServerHttpUtils::MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue),
		PendingMessages.Num());
	OnComplete(MoveTemp(SseResponse));
	return true;
}

bool FUEMCPServerMcpServer::HandleGetRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FGuid SessionId;
	const FString SessionIdHeaderValue = UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::SessionIdHeader);
	bool bHasSession = UEMCPServerHttpUtils::TryParseSessionId(SessionIdHeaderValue, SessionId);
	if (!bHasSession)
	{
		if (const FString* QueryValue = Request.QueryParams.Find(TEXT("sessionId")))
		{
			bHasSession = UEMCPServerHttpUtils::TryParseSessionId(*QueryValue, SessionId);
		}
		else if (const FString* QueryValueLegacy = Request.QueryParams.Find(TEXT("session_id")))
		{
			bHasSession = UEMCPServerHttpUtils::TryParseSessionId(*QueryValueLegacy, SessionId);
		}
	}

	TSharedPtr<FUEMCPServerMcpSession> Session;
	const FString Endpoint = UEMCPServerHttpUtils::PeerEndpointString(Request.PeerAddress);
	const FString AcceptHeaderValue = UEMCPServerHttpUtils::ExtractHeaderValue(Request.Headers, UEMCPServer::AcceptHeader);

	if (bHasSession)
	{
		Session = FindSessionById(SessionId);
		if (Session.IsValid())
		{
			AssociateEndpointWithSession(Endpoint, SessionId);
			UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> GET SSE reuse session"),
				*UEMCPServerHttpUtils::MakeLogContext(TEXT("GET"), Endpoint, SessionId, FString(), AcceptHeaderValue));
		}
	}

	bool bCreatedSession = false;
	if (!Session.IsValid())
	{
		Session = CreateSession(Endpoint, SessionId);
		bCreatedSession = true;
	}

	static const FString KeepAlivePayload(TEXT(": keep-alive\n\n"));

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(KeepAlivePayload, UEMCPServer::ContentTypeEventStreamResponse);
	Response->Headers.Add(UEMCPServer::CacheControlHeader, { UEMCPServer::NoStoreValue });
	Response->Headers.Add(UEMCPServer::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
	Response->Headers.Add(UEMCPServer::ProtocolVersionHeader, { UEMCPServer::ProtocolVersionValue });
	UE_LOG(LogUEMCPServer, Verbose, TEXT("%s -> GET SSE %s"),
		*UEMCPServerHttpUtils::MakeLogContext(TEXT("GET"), Endpoint, SessionId, FString(), AcceptHeaderValue),
		bCreatedSession ? TEXT("created new session") : TEXT("keep-alive"));
	OnComplete(MoveTemp(Response));
	return true;
}

TSharedPtr<FUEMCPServerMcpSession> FUEMCPServerMcpServer::FindSessionById(const FGuid& ClientId)
{
	FScopeLock Guard(&SessionMutex);
	if (const TSharedPtr<FUEMCPServerMcpSession>* SessionPtr = Sessions.Find(ClientId))
	{
		return *SessionPtr;
	}
	return nullptr;
}

TSharedPtr<FUEMCPServerMcpSession> FUEMCPServerMcpServer::CreateSession(const FString& Endpoint, FGuid& OutSessionId)
{
	FScopeLock Guard(&SessionMutex);
	OutSessionId = FGuid::NewGuid();

	TSharedPtr<FUEMCPServerMcpSession> Session = MakeShared<FUEMCPServerMcpSession>(LiveCodingManager, OutSessionId, Endpoint);
	Sessions.Add(OutSessionId, Session);
	EndpointToSession.Add(Endpoint, OutSessionId);

	UE_LOG(LogUEMCPServer, Display, TEXT("MCP session created for client %s (%s)."), *OutSessionId.ToString(EGuidFormats::DigitsWithHyphens), *Endpoint);
	return Session;
}

TSharedPtr<FUEMCPServerMcpSession> FUEMCPServerMcpServer::FindSessionForEndpoint(const FString& Endpoint, FGuid& OutSessionId)
{
	FScopeLock Guard(&SessionMutex);
	if (const FGuid* SessionIdPtr = EndpointToSession.Find(Endpoint))
	{
		if (TSharedPtr<FUEMCPServerMcpSession>* SessionPtr = Sessions.Find(*SessionIdPtr))
		{
			OutSessionId = *SessionIdPtr;
			return *SessionPtr;
		}
	}
	return nullptr;
}

TSharedPtr<FUEMCPServerMcpSession> FUEMCPServerMcpServer::FindDefaultSession(FGuid& OutSessionId)
{
	FScopeLock Guard(&SessionMutex);
	if (Sessions.Num() == 1)
	{
		for (const TPair<FGuid, TSharedPtr<FUEMCPServerMcpSession>>& Pair : Sessions)
		{
			OutSessionId = Pair.Key;
			return Pair.Value;
		}
	}
	return nullptr;
}

void FUEMCPServerMcpServer::AssociateEndpointWithSession(const FString& Endpoint, const FGuid& SessionId)
{
	FScopeLock Guard(&SessionMutex);
	EndpointToSession.Add(Endpoint, SessionId);
}

bool FUEMCPServerMcpServer::ValidateProtocolVersion(const FString& ProtocolVersionHeader) const
{
	if (ProtocolVersionHeader.IsEmpty())
	{
		return true;
	}

	return ProtocolVersionHeader.Equals(UEMCPServer::ProtocolVersionValue, ESearchCase::CaseSensitive)
		|| ProtocolVersionHeader.Equals(TEXT("2025-03-26"), ESearchCase::CaseSensitive)
		|| ProtocolVersionHeader.Equals(TEXT("2024-11-05"), ESearchCase::CaseSensitive);
}

void FUEMCPServerMcpServer::SetSessionOverrideConfig() const
{
	if (!GConfig)
	{
		return;
	}

	const FString BindValue = BindAddress.IsEmpty() ? TEXT("127.0.0.1") : BindAddress;

	TArray<FString> Overrides;
	GConfig->GetArray(UEMCPServer::HttpListenersSection, UEMCPServer::ListenerOverridesKey, Overrides, GEngineIni);

	const FString DesiredEntry = FString::Printf(TEXT("(Port=%u,BindAddress=%s)"), Port, *BindValue);
	bool bUpdated = false;
	for (FString& Existing : Overrides)
	{
		if (Existing.Contains(FString::Printf(TEXT("Port=%u"), Port)))
		{
			Existing = DesiredEntry;
			bUpdated = true;
			break;
		}
	}

	if (!bUpdated)
	{
		Overrides.Add(DesiredEntry);
	}

	GConfig->SetArray(UEMCPServer::HttpListenersSection, UEMCPServer::ListenerOverridesKey, Overrides, GEngineIni);
}


