#pragma once

#include "CoreMinimal.h"
#include "UEMCPServerLiveCodingTypes.h"

#include "IUEMCPServerLiveCodingProvider.h"

#include "HAL/CriticalSection.h"
#include "Templates/Atomic.h"

enum class ELiveCodingCompileResult : uint8;

class FUEMCPServerLiveCodingLogCapture;

/**
 * Owns the Live Coding compile flow and maintains the latest log snapshot.
 */
class FUEMCPServerLiveCodingManager : public IUEMCPServerLiveCodingProvider
{
public:
	FUEMCPServerLiveCodingManager();
	~FUEMCPServerLiveCodingManager();

	void Initialize();
	void Shutdown();

	/** Attempts to begin a compile; returns false if one is already running or setup failed. */
	virtual bool TryBeginCompile(FString& OutErrorMessage) override;

	/** Executes the Live Coding compile synchronously. Must be called on the game thread. */
	virtual void ExecuteCompileOnGameThread() override;

	/** Retrieves the latest compile snapshot and status information. */
	virtual void GetLastCompileSnapshot(TArray<FUEMCPServerLogEntry>& OutEntries, FDateTime& OutTimestamp, ELiveCodingCompileResult& OutResult, bool& bOutHasResult, FString& OutErrorMessage, bool& bOutIsInProgress) const override;

private:
	bool EnsureCaptureAvailable(FString& OutErrorMessage);
	bool EnsureLiveCodingAvailable(FString& OutErrorMessage, class ILiveCodingModule*& OutModule) const;
	void FinalizeCompile(TArray<FUEMCPServerLogEntry>&& CapturedEntries, ELiveCodingCompileResult Result, const FString& ErrorMessage);
	void FinalizeCompileWithError(const FString& ErrorMessage, ELiveCodingCompileResult Result);

private:
	TUniquePtr<FUEMCPServerLiveCodingLogCapture> LogCapture;
	mutable FCriticalSection LogMutex;
	TArray<FUEMCPServerLogEntry> LastCompileLogEntries;
	FDateTime LastCompileTimestamp;
	ELiveCodingCompileResult LastCompileResult;
	bool bHasCompileResult;
	TAtomic<bool> bCompileInProgress;
	FString LastErrorMessage;
};
