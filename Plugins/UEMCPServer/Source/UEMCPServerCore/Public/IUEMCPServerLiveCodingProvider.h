#pragma once

#include "CoreMinimal.h"
#include "UEMCPServerLiveCodingTypes.h"
#include "ILiveCodingModule.h"

/**
 * Interface for providing Live Coding functionality to the MCP server.
 */
class UEMCPSERVERCORE_API IUEMCPServerLiveCodingProvider
{
public:
	virtual ~IUEMCPServerLiveCodingProvider() = default;

	/** Attempts to begin a compile; returns false if one is already running or setup failed. */
	virtual bool TryBeginCompile(FString& OutErrorMessage) = 0;

	/** Executes the Live Coding compile synchronously. Must be called on the game thread. */
	virtual void ExecuteCompileOnGameThread() = 0;

	/** Retrieves the latest compile snapshot and status information. */
	virtual void GetLastCompileSnapshot(TArray<FUEMCPServerLogEntry>& OutEntries, FDateTime& OutTimestamp, ELiveCodingCompileResult& OutResult, bool& bOutHasResult, FString& OutErrorMessage, bool& bOutIsInProgress) const = 0;
};
