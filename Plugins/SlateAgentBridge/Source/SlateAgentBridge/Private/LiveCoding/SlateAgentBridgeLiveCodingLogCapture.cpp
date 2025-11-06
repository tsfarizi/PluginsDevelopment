#include "SlateAgentBridgeLiveCodingLogCapture.h"

#include "Logging/LogVerbosity.h"
#include "Misc/DateTime.h"

FSlateAgentBridgeLiveCodingLogCapture::FSlateAgentBridgeLiveCodingLogCapture() = default;

void FSlateAgentBridgeLiveCodingLogCapture::StartCapture()
{
	FScopeLock CaptureLock(&CaptureMutex);
	CapturedEntries.Reset();
	bIsCapturing = true;
}

TArray<FSlateAgentBridgeLogEntry> FSlateAgentBridgeLiveCodingLogCapture::StopCapture()
{
	FScopeLock CaptureLock(&CaptureMutex);
	bIsCapturing = false;
	return MoveTemp(CapturedEntries);
}

void FSlateAgentBridgeLiveCodingLogCapture::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (Category.IsNone())
	{
		return;
	}

	const FString CategoryString = Category.ToString();
	if (!CategoryString.Contains(TEXT("LiveCoding")))
	{
		return;
	}

	FScopeLock CaptureLockInstance(&CaptureMutex);
	if (!bIsCapturing)
	{
		return;
	}

	FSlateAgentBridgeLogEntry& NewEntry = CapturedEntries.AddDefaulted_GetRef();
	NewEntry.Category = CategoryString;
	NewEntry.Message = FString(V);
	NewEntry.Verbosity = FString(::ToString(Verbosity));
	NewEntry.Timestamp = FDateTime::UtcNow();
}
