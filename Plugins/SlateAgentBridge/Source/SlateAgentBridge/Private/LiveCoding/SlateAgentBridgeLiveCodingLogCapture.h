#pragma once

#include "CoreMinimal.h"
#include "SlateAgentBridgeLiveCodingTypes.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDevice.h"

/**
 * Captures Live Coding logs while a compile is in-flight.
 */
class FSlateAgentBridgeLiveCodingLogCapture : public FOutputDevice
{
public:
	FSlateAgentBridgeLiveCodingLogCapture();

	void StartCapture();
	TArray<FSlateAgentBridgeLogEntry> StopCapture();

	//~ Begin FOutputDevice Interface
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
	//~ End FOutputDevice Interface

private:
	FCriticalSection CaptureMutex;
	bool bIsCapturing = false;
	TArray<FSlateAgentBridgeLogEntry> CapturedEntries;
};
