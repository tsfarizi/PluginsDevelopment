#include "UEMCPServerLiveCodingLogCapture.h"

#include "Logging/LogVerbosity.h"
#include "Misc/DateTime.h"

FUEMCPServerLiveCodingLogCapture::FUEMCPServerLiveCodingLogCapture() = default;

void FUEMCPServerLiveCodingLogCapture::StartCapture()
{
	FScopeLock CaptureLock(&CaptureMutex);
	CapturedEntries.Reset();
	bIsCapturing = true;
}

TArray<FUEMCPServerLogEntry> FUEMCPServerLiveCodingLogCapture::StopCapture()
{
	FScopeLock CaptureLock(&CaptureMutex);
	bIsCapturing = false;
	return MoveTemp(CapturedEntries);
}

void FUEMCPServerLiveCodingLogCapture::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
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

	FUEMCPServerLogEntry& NewEntry = CapturedEntries.AddDefaulted_GetRef();
	NewEntry.Category = CategoryString;
	NewEntry.Message = FString(V);
	NewEntry.Verbosity = FString(::ToString(Verbosity));
	NewEntry.Timestamp = FDateTime::UtcNow();
}
