#pragma once

#include "CoreMinimal.h"
#include "UEMCPServerLiveCodingTypes.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDevice.h"

/**
 * Captures Live Coding logs while a compile is in-flight.
 */
class FUEMCPServerLiveCodingLogCapture : public FOutputDevice
{
public:
	FUEMCPServerLiveCodingLogCapture();

	void StartCapture();
	TArray<FUEMCPServerLogEntry> StopCapture();

	//~ Begin FOutputDevice Interface
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
	//~ End FOutputDevice Interface

private:
	FCriticalSection CaptureMutex;
	bool bIsCapturing = false;
	TArray<FUEMCPServerLogEntry> CapturedEntries;
};
