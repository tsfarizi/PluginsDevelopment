#include "Mcp/UEMCPServerHttpUtils.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString UEMCPServerHttpUtils::RequestBodyToString(const FHttpServerRequest& Request)
{
	if (Request.Body.IsEmpty())
	{
		return FString();
	}

	const ANSICHAR* Utf8Data = reinterpret_cast<const ANSICHAR*>(Request.Body.GetData());
	FUTF8ToTCHAR Converter(Utf8Data, Request.Body.Num());
	return FString(Converter.Length(), Converter.Get());
}

FString UEMCPServerHttpUtils::PeerEndpointString(const TSharedPtr<FInternetAddr>& PeerAddress)
{
	return PeerAddress.IsValid() ? PeerAddress->ToString(true) : FString(TEXT("unknown"));
}

bool UEMCPServerHttpUtils::ContainsToken(const FString& Source, const FString& Token)
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

TSharedPtr<FJsonObject> UEMCPServerHttpUtils::ParseJsonObject(const FString& Body)
{
	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
	{
		return Object;
	}
	return nullptr;
}

FString UEMCPServerHttpUtils::MakeLogContext(const TCHAR* Phase, const FString& Endpoint, const FGuid& SessionId, const FString& Method, const FString& Accept)
{
	const FString SessionString = SessionId.IsValid() ? SessionId.ToString(EGuidFormats::DigitsWithHyphens) : FString(TEXT("<none>"));
	const FString EndpointString = Endpoint.IsEmpty() ? FString(TEXT("unknown")) : Endpoint;
	const FString MethodString = Method.IsEmpty() ? FString(TEXT("<none>")) : Method;
	const FString AcceptString = Accept.IsEmpty() ? FString(TEXT("<none>")) : Accept;
	return FString::Printf(TEXT("%s endpoint=%s method=%s session=%s accept=%s"), Phase, *EndpointString, *MethodString, *SessionString, *AcceptString);
}

void UEMCPServerHttpUtils::AppendSseEvent(FString& Output, const FString& Message)
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

FString UEMCPServerHttpUtils::ExtractHeaderValue(const TMap<FString, TArray<FString>>& Headers, const FString& HeaderName)
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

bool UEMCPServerHttpUtils::TryParseSessionId(const FString& RawValue, FGuid& OutSessionId)
{
	if (RawValue.IsEmpty())
	{
		return false;
	}

	return FGuid::Parse(RawValue, OutSessionId);
}
