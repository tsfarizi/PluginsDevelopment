#pragma once

#include "CoreMinimal.h"
#include "SlateAgentBridgeLiveCodingTypes.h"

#include "HAL/CriticalSection.h"
#include "Templates/Atomic.h"

enum class ELiveCodingCompileResult : uint8;

class FSlateAgentBridgeLiveCodingLogCapture;

/**
 * Owns the Live Coding compile flow and maintains the latest log snapshot.
 */
class FSlateAgentBridgeLiveCodingManager
{
public:
	FSlateAgentBridgeLiveCodingManager();
	~FSlateAgentBridgeLiveCodingManager();

	void Initialize();
	void Shutdown();

	/** Attempts to begin a compile; returns false if one is already running or setup failed. */
	bool TryBeginCompile(FString& OutErrorMessage);

	/** Executes the Live Coding compile synchronously. Must be called on the game thread. */
	void ExecuteCompileOnGameThread();

	/** Retrieves the latest compile snapshot and status information. */
	void GetLastCompileSnapshot(TArray<FSlateAgentBridgeLogEntry>& OutEntries, FDateTime& OutTimestamp, ELiveCodingCompileResult& OutResult, bool& bOutHasResult, FString& OutErrorMessage, bool& bOutIsInProgress) const;

	static FString CompileResultToString(ELiveCodingCompileResult CompileResult);

private:
	bool EnsureCaptureAvailable(FString& OutErrorMessage);
	bool EnsureLiveCodingAvailable(FString& OutErrorMessage, class ILiveCodingModule*& OutModule) const;
	void FinalizeCompile(TArray<FSlateAgentBridgeLogEntry>&& CapturedEntries, ELiveCodingCompileResult Result, const FString& ErrorMessage);
	void FinalizeCompileWithError(const FString& ErrorMessage, ELiveCodingCompileResult Result);

private:
	TUniquePtr<FSlateAgentBridgeLiveCodingLogCapture> LogCapture;
	mutable FCriticalSection LogMutex;
	TArray<FSlateAgentBridgeLogEntry> LastCompileLogEntries;
	FDateTime LastCompileTimestamp;
	ELiveCodingCompileResult LastCompileResult;
	bool bHasCompileResult;
	TAtomic<bool> bCompileInProgress;
	FString LastErrorMessage;
};
