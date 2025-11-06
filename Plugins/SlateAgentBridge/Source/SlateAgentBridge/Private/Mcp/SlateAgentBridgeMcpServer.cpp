#include "Mcp/SlateAgentBridgeMcpServer.h"

#include "Mcp/SlateAgentBridgeMcpSession.h"
#include "LiveCoding/SlateAgentBridgeLiveCodingManager.h"
#include "SlateAgentBridgeLog.h"

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

namespace SlateAgentBridge
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

namespace
{
	FString RequestBodyToString(const FHttpServerRequest& Request)
	{
		if (Request.Body.IsEmpty())
		{
			return FString();
		}

		const ANSICHAR* Utf8Data = reinterpret_cast<const ANSICHAR*>(Request.Body.GetData());
		FUTF8ToTCHAR Converter(Utf8Data, Request.Body.Num());
		return FString(Converter.Length(), Converter.Get());
	}

	FString PeerEndpointString(const TSharedPtr<FInternetAddr>& PeerAddress)
	{
		return PeerAddress.IsValid() ? PeerAddress->ToString(true) : FString(TEXT("unknown"));
	}

bool ContainsToken(const FString& Source, const FString& Token)
{
	TArray<FString> Parts;
	Source.ParseIntoArray(Parts, TEXT(","), true);
	for (FString& Part : Parts)
	{
		Part.TrimStartAndEndInline();
		FString TokenPart;
		FString Remainder;
		if (Part.Split(TEXT(";"), &TokenPart, &Remainder))
		{
			TokenPart.TrimEndInline();
		}
		else
		{
			TokenPart = Part;
		}

		if (TokenPart.Equals(Token, ESearchCase::IgnoreCase))
		{
			return true;
		}

		if (TokenPart.Equals(TEXT("*/*"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		int32 SlashIndexCandidate = INDEX_NONE;
		int32 SlashIndexTarget = INDEX_NONE;
		if (TokenPart.FindChar(TEXT('/'), SlashIndexCandidate) && Token.FindChar(TEXT('/'), SlashIndexTarget))
		{
			const FString CandidateType = TokenPart.Left(SlashIndexCandidate);
			const FString CandidateSubType = TokenPart.Mid(SlashIndexCandidate + 1);
			const FString TargetType = Token.Left(SlashIndexTarget);
			const FString TargetSubType = Token.Mid(SlashIndexTarget + 1);

			if (CandidateType.Equals(TargetType, ESearchCase::IgnoreCase) && CandidateSubType.Equals(TEXT("*"), ESearchCase::IgnoreCase))
			{
				return true;
			}

			if (CandidateType.Equals(TEXT("*"), ESearchCase::IgnoreCase) && CandidateSubType.Equals(TargetSubType, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (TokenPart.Equals(TEXT("*"), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
	}

	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Body)
	{
		TSharedPtr<FJsonObject> Object;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
		{
			return Object;
	}
	return nullptr;
	}

	FString MakeLogContext(const TCHAR* Phase, const FString& Endpoint, const FGuid& SessionId, const FString& Method, const FString& Accept)
	{
		const FString SessionString = SessionId.IsValid() ? SessionId.ToString(EGuidFormats::DigitsWithHyphens) : FString(TEXT("<none>"));
		const FString EndpointString = Endpoint.IsEmpty() ? FString(TEXT("unknown")) : Endpoint;
		const FString MethodString = Method.IsEmpty() ? FString(TEXT("<none>")) : Method;
		const FString AcceptString = Accept.IsEmpty() ? FString(TEXT("<none>")) : Accept;
		return FString::Printf(TEXT("%s endpoint=%s method=%s session=%s accept=%s"), Phase, *EndpointString, *MethodString, *SessionString, *AcceptString);
	}

	void AppendSseEvent(FString& Output, const FString& Message)
	{
		FString Normalized = Message;
		Normalized.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Normalized.ReplaceInline(TEXT("\r"), TEXT("\n"));

		TArray<FString> Lines;
		Normalized.ParseIntoArrayLines(Lines);
		if (Lines.IsEmpty())
		{
			Lines.Add(FString());
		}

		for (const FString& Line : Lines)
		{
			Output += TEXT("data: ");
			Output += Line;
			Output += TEXT("\n");
		}

		Output += TEXT("\n");
	}
}

FSlateAgentBridgeMcpServer::FSlateAgentBridgeMcpServer(FSlateAgentBridgeLiveCodingManager& InLiveCodingManager, uint32 InPort, const FString& InBindAddress)
	: LiveCodingManager(InLiveCodingManager)
	, Port(InPort)
	, BindAddress(InBindAddress)
	, EndpointPath(SlateAgentBridge::DefaultMcpEndpointPath)
	, bListenersStarted(false)
{
}

FSlateAgentBridgeMcpServer::~FSlateAgentBridgeMcpServer()
{
	Stop();
}

bool FSlateAgentBridgeMcpServer::Start()
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
		UE_LOG(LogSlateAgentBridge, Error, TEXT("Unable to start MCP HTTP server on %s:%u"), BindAddress.IsEmpty() ? TEXT("0.0.0.0") : *BindAddress, Port);
		return false;
	}

	const FHttpPath EndpointPathObject(EndpointPath);

	PostRouteHandle = Router->BindRoute(
		EndpointPathObject,
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FSlateAgentBridgeMcpServer::HandlePostRequest));

	if (!PostRouteHandle.IsValid())
	{
		UE_LOG(LogSlateAgentBridge, Error, TEXT("Failed to bind MCP POST handler at %s"), *EndpointPathObject.GetPath());
		Router.Reset();
		return false;
	}

	GetRouteHandle = Router->BindRoute(
		EndpointPathObject,
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FSlateAgentBridgeMcpServer::HandleGetRequest));

	if (!GetRouteHandle.IsValid())
	{
		UE_LOG(LogSlateAgentBridge, Error, TEXT("Failed to bind MCP GET handler at %s"), *EndpointPathObject.GetPath());
		Router->UnbindRoute(PostRouteHandle);
		Router.Reset();
		return false;
	}

	if (!bListenersStarted)
	{
		HttpModule.StartAllListeners();
		bListenersStarted = true;
	}

	UE_LOG(LogSlateAgentBridge, Display, TEXT("SlateAgentBridge MCP server listening on http://%s:%u%s"),
		BindAddress.IsEmpty() ? TEXT("127.0.0.1") : *BindAddress,
		Port,
		*EndpointPathObject.GetPath());

	return true;
}

void FSlateAgentBridgeMcpServer::Stop()
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

bool FSlateAgentBridgeMcpServer::HandlePostRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString Body = RequestBodyToString(Request);
	if (Body.IsEmpty())
	{
		UE_LOG(LogSlateAgentBridge, Warning, TEXT("%s -> rejecting: empty body"),
			*MakeLogContext(TEXT("POST"), PeerEndpointString(Request.PeerAddress), FGuid(), FString(), ExtractHeaderValue(Request.Headers, SlateAgentBridge::AcceptHeader)));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("empty_body"), TEXT("Request body is required.")));
		return true;
	}

	const FString ProtocolVersionHeaderValue = ExtractHeaderValue(Request.Headers, SlateAgentBridge::ProtocolVersionHeader);
	if (!ValidateProtocolVersion(ProtocolVersionHeaderValue))
	{
		UE_LOG(LogSlateAgentBridge, Warning, TEXT("%s -> rejecting: unsupported protocol %s"),
			*MakeLogContext(TEXT("POST"), PeerEndpointString(Request.PeerAddress), FGuid(), FString(), ExtractHeaderValue(Request.Headers, SlateAgentBridge::AcceptHeader)),
			*ProtocolVersionHeaderValue);
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_protocol_version"), TEXT("Unsupported MCP protocol version.")));
		return true;
	}

	TSharedPtr<FJsonObject> JsonObject = ParseJsonObject(Body);
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogSlateAgentBridge, Warning, TEXT("%s -> rejecting: invalid JSON"),
			*MakeLogContext(TEXT("POST"), PeerEndpointString(Request.PeerAddress), FGuid(), FString(), ExtractHeaderValue(Request.Headers, SlateAgentBridge::AcceptHeader)));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json"), TEXT("Failed to parse JSON-RPC payload.")));
		return true;
	}

	const FString AcceptHeaderValue = ExtractHeaderValue(Request.Headers, SlateAgentBridge::AcceptHeader);
	const bool bClientAcceptsJson = AcceptHeaderValue.IsEmpty() || ContainsToken(AcceptHeaderValue, SlateAgentBridge::ContentTypeJson);
	const bool bClientAcceptsSse = ContainsToken(AcceptHeaderValue, SlateAgentBridge::ContentTypeEventStream);
	if (!bClientAcceptsJson && !bClientAcceptsSse)
	{
		UE_LOG(LogSlateAgentBridge, Warning, TEXT("%s -> rejecting: unsupported Accept"),
			*MakeLogContext(TEXT("POST"), PeerEndpointString(Request.PeerAddress), FGuid(), FString(), AcceptHeaderValue));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NoneAcceptable, TEXT("unsupported_accept"), TEXT("Client must accept application/json or text/event-stream.")));
		return true;
	}

	const FString Endpoint = PeerEndpointString(Request.PeerAddress);

	FGuid SessionId;
	const FString SessionIdHeaderValue = ExtractHeaderValue(Request.Headers, SlateAgentBridge::SessionIdHeader);
	const bool bHasSessionHeader = TryParseSessionId(SessionIdHeaderValue, SessionId);

	bool bIsInitializeRequest = false;
	FString Method;
	if (JsonObject->TryGetStringField(TEXT("method"), Method))
	{
		bIsInitializeRequest = Method.Equals(TEXT("initialize"), ESearchCase::CaseSensitive);
	}

	UE_LOG(LogSlateAgentBridge, Verbose, TEXT("MCP POST %s from %s (Accept=%s, HasSessionHeader=%s)"),
		Method.IsEmpty() ? TEXT("<response>") : *Method,
		Endpoint.IsEmpty() ? TEXT("unknown") : *Endpoint,
		AcceptHeaderValue.IsEmpty() ? TEXT("<none>") : *AcceptHeaderValue,
		bHasSessionHeader ? TEXT("true") : TEXT("false"));

	TSharedPtr<FSlateAgentBridgeMcpSession> Session;
	if (bHasSessionHeader)
	{
		Session = FindSessionById(SessionId);
		if (!Session.IsValid())
		{
			OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("unknown_session"), TEXT("MCP session not found.")));
			return true;
		}
		AssociateEndpointWithSession(Endpoint, SessionId);
		UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> using header session"),
			*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
	}
	else if (bIsInitializeRequest)
	{
		Session = FindSessionForEndpoint(Endpoint, SessionId);
		if (Session.IsValid())
		{
			UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> initialize reuse endpoint session"),
				*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		}
		if (!Session.IsValid())
		{
			Session = FindDefaultSession(SessionId);
			if (Session.IsValid())
			{
				AssociateEndpointWithSession(Endpoint, SessionId);
				UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> initialize reuse default session"),
					*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
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
			UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> reuse endpoint session"),
				*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		}
		if (!Session.IsValid())
		{
			Session = FindDefaultSession(SessionId);
			if (Session.IsValid())
			{
				AssociateEndpointWithSession(Endpoint, SessionId);
				UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> request reuse default session"),
					*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
			}
		}

		if (!Session.IsValid())
		{
			UE_LOG(LogSlateAgentBridge, Warning, TEXT("%s -> rejecting: session missing"),
				*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
			OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("missing_session"), TEXT("Mcp-Session-Id header is required.")));
			return true;
		}
	}

	TArray<FString> PendingMessages;
	if (!Session->HandleMessage(Body, PendingMessages))
	{
		UE_LOG(LogSlateAgentBridge, Warning, TEXT("%s -> session processing error"),
			*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::ServerError, TEXT("session_error"), TEXT("Failed to process MCP message.")));
		return true;
	}

	if (PendingMessages.IsEmpty())
	{
		TUniquePtr<FHttpServerResponse> AcceptedResponse = MakeUnique<FHttpServerResponse>();
		AcceptedResponse->Code = EHttpServerResponseCodes::Accepted;
		AcceptedResponse->Headers.Add(SlateAgentBridge::CacheControlHeader, { SlateAgentBridge::NoStoreValue });
		if (SessionId.IsValid())
		{
			AcceptedResponse->Headers.Add(SlateAgentBridge::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
		}
		AcceptedResponse->Headers.Add(SlateAgentBridge::ProtocolVersionHeader, { SlateAgentBridge::ProtocolVersionValue });
		UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> returning 202 Accepted"),
			*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(MoveTemp(AcceptedResponse));
		return true;
	}

	if (PendingMessages.Num() == 1 && bClientAcceptsJson)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(PendingMessages[0], SlateAgentBridge::ContentTypeJson);
		Response->Headers.Add(SlateAgentBridge::CacheControlHeader, { SlateAgentBridge::NoStoreValue });
		if (SessionId.IsValid())
		{
			Response->Headers.Add(SlateAgentBridge::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
		}
		Response->Headers.Add(SlateAgentBridge::ProtocolVersionHeader, { SlateAgentBridge::ProtocolVersionValue });
		UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> returning JSON response"),
			*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(MoveTemp(Response));
		return true;
	}

	if (!bClientAcceptsSse)
	{
		UE_LOG(LogSlateAgentBridge, Warning, TEXT("%s -> rejecting: SSE required for multi-message response"),
			*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NoneAcceptable, TEXT("sse_required"), TEXT("Client must accept text/event-stream for multi-message responses.")));
		return true;
	}

	FString SsePayload;
	SsePayload.Reserve(PendingMessages.Num() * 64);
	for (const FString& Message : PendingMessages)
	{
		AppendSseEvent(SsePayload, Message);
	}

	TUniquePtr<FHttpServerResponse> SseResponse = FHttpServerResponse::Create(SsePayload, SlateAgentBridge::ContentTypeEventStreamResponse);
	SseResponse->Headers.Add(SlateAgentBridge::CacheControlHeader, { SlateAgentBridge::NoStoreValue });
	if (SessionId.IsValid())
	{
		SseResponse->Headers.Add(SlateAgentBridge::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
	}
	SseResponse->Headers.Add(SlateAgentBridge::ProtocolVersionHeader, { SlateAgentBridge::ProtocolVersionValue });
	UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> returning SSE (%d message(s))"),
		*MakeLogContext(TEXT("POST"), Endpoint, SessionId, Method, AcceptHeaderValue),
		PendingMessages.Num());
	OnComplete(MoveTemp(SseResponse));
	return true;
}

bool FSlateAgentBridgeMcpServer::HandleGetRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FGuid SessionId;
	const FString SessionIdHeaderValue = ExtractHeaderValue(Request.Headers, SlateAgentBridge::SessionIdHeader);
	bool bHasSession = TryParseSessionId(SessionIdHeaderValue, SessionId);
	if (!bHasSession)
	{
		if (const FString* QueryValue = Request.QueryParams.Find(TEXT("sessionId")))
		{
			bHasSession = TryParseSessionId(*QueryValue, SessionId);
		}
		else if (const FString* QueryValueLegacy = Request.QueryParams.Find(TEXT("session_id")))
		{
			bHasSession = TryParseSessionId(*QueryValueLegacy, SessionId);
		}
	}

	TSharedPtr<FSlateAgentBridgeMcpSession> Session;
	const FString Endpoint = PeerEndpointString(Request.PeerAddress);
	const FString AcceptHeaderValue = ExtractHeaderValue(Request.Headers, SlateAgentBridge::AcceptHeader);

	if (bHasSession)
	{
		Session = FindSessionById(SessionId);
		if (Session.IsValid())
		{
			AssociateEndpointWithSession(Endpoint, SessionId);
			UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> GET SSE reuse session"),
				*MakeLogContext(TEXT("GET"), Endpoint, SessionId, FString(), AcceptHeaderValue));
		}
	}

	bool bCreatedSession = false;
	if (!Session.IsValid())
	{
		Session = CreateSession(Endpoint, SessionId);
		bCreatedSession = true;
	}

	static const FString KeepAlivePayload(TEXT(": keep-alive\n\n"));

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(KeepAlivePayload, SlateAgentBridge::ContentTypeEventStreamResponse);
	Response->Headers.Add(SlateAgentBridge::CacheControlHeader, { SlateAgentBridge::NoStoreValue });
	Response->Headers.Add(SlateAgentBridge::SessionIdHeader, { SessionId.ToString(EGuidFormats::DigitsWithHyphens) });
	Response->Headers.Add(SlateAgentBridge::ProtocolVersionHeader, { SlateAgentBridge::ProtocolVersionValue });
	UE_LOG(LogSlateAgentBridge, Verbose, TEXT("%s -> GET SSE %s"),
		*MakeLogContext(TEXT("GET"), Endpoint, SessionId, FString(), AcceptHeaderValue),
		bCreatedSession ? TEXT("created new session") : TEXT("keep-alive"));
	OnComplete(MoveTemp(Response));
	return true;
}

TSharedPtr<FSlateAgentBridgeMcpSession> FSlateAgentBridgeMcpServer::FindSessionById(const FGuid& ClientId)
{
	FScopeLock Guard(&SessionMutex);
	if (const TSharedPtr<FSlateAgentBridgeMcpSession>* SessionPtr = Sessions.Find(ClientId))
	{
		return *SessionPtr;
	}
	return nullptr;
}

TSharedPtr<FSlateAgentBridgeMcpSession> FSlateAgentBridgeMcpServer::CreateSession(const FString& Endpoint, FGuid& OutSessionId)
{
	FScopeLock Guard(&SessionMutex);
	OutSessionId = FGuid::NewGuid();

	TSharedPtr<FSlateAgentBridgeMcpSession> Session = MakeShared<FSlateAgentBridgeMcpSession>(LiveCodingManager, OutSessionId, Endpoint);
	Sessions.Add(OutSessionId, Session);
	EndpointToSession.Add(Endpoint, OutSessionId);

	UE_LOG(LogSlateAgentBridge, Display, TEXT("MCP session created for client %s (%s)."), *OutSessionId.ToString(EGuidFormats::DigitsWithHyphens), *Endpoint);
	return Session;
}

TSharedPtr<FSlateAgentBridgeMcpSession> FSlateAgentBridgeMcpServer::FindSessionForEndpoint(const FString& Endpoint, FGuid& OutSessionId)
{
	FScopeLock Guard(&SessionMutex);
	if (const FGuid* SessionIdPtr = EndpointToSession.Find(Endpoint))
	{
		if (TSharedPtr<FSlateAgentBridgeMcpSession>* SessionPtr = Sessions.Find(*SessionIdPtr))
		{
			OutSessionId = *SessionIdPtr;
			return *SessionPtr;
		}
	}
	return nullptr;
}

TSharedPtr<FSlateAgentBridgeMcpSession> FSlateAgentBridgeMcpServer::FindDefaultSession(FGuid& OutSessionId)
{
	FScopeLock Guard(&SessionMutex);
	if (Sessions.Num() == 1)
	{
		for (const TPair<FGuid, TSharedPtr<FSlateAgentBridgeMcpSession>>& Pair : Sessions)
		{
			OutSessionId = Pair.Key;
			return Pair.Value;
		}
	}
	return nullptr;
}

void FSlateAgentBridgeMcpServer::AssociateEndpointWithSession(const FString& Endpoint, const FGuid& SessionId)
{
	FScopeLock Guard(&SessionMutex);
	EndpointToSession.Add(Endpoint, SessionId);
}

bool FSlateAgentBridgeMcpServer::ValidateProtocolVersion(const FString& ProtocolVersionHeader) const
{
	if (ProtocolVersionHeader.IsEmpty())
	{
		return true;
	}

	return ProtocolVersionHeader.Equals(SlateAgentBridge::ProtocolVersionValue, ESearchCase::CaseSensitive)
		|| ProtocolVersionHeader.Equals(TEXT("2025-03-26"), ESearchCase::CaseSensitive)
		|| ProtocolVersionHeader.Equals(TEXT("2024-11-05"), ESearchCase::CaseSensitive);
}

void FSlateAgentBridgeMcpServer::SetSessionOverrideConfig() const
{
	if (!GConfig)
	{
		return;
	}

	const FString BindValue = BindAddress.IsEmpty() ? TEXT("127.0.0.1") : BindAddress;

	TArray<FString> Overrides;
	GConfig->GetArray(SlateAgentBridge::HttpListenersSection, SlateAgentBridge::ListenerOverridesKey, Overrides, GEngineIni);

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

	GConfig->SetArray(SlateAgentBridge::HttpListenersSection, SlateAgentBridge::ListenerOverridesKey, Overrides, GEngineIni);
}

bool FSlateAgentBridgeMcpServer::TryParseSessionId(const FString& RawValue, FGuid& OutSessionId)
{
	if (RawValue.IsEmpty())
	{
		return false;
	}

	return FGuid::Parse(RawValue, OutSessionId);
}

FString FSlateAgentBridgeMcpServer::ExtractHeaderValue(const TMap<FString, TArray<FString>>& Headers, const FString& HeaderName)
{
	for (const TPair<FString, TArray<FString>>& Pair : Headers)
	{
		if (!Pair.Key.Equals(HeaderName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		for (const FString& Value : Pair.Value)
		{
			if (!Value.IsEmpty())
			{
				return Value;
			}
		}
	}
	return FString();
}
