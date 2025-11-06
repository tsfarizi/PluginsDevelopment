#include "SlateAgentBridgeLiveCodingManager.h"

#include "SlateAgentBridgeLiveCodingLogCapture.h"
#include "SlateAgentBridgeLog.h"

#include "ILiveCodingModule.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

FSlateAgentBridgeLiveCodingManager::FSlateAgentBridgeLiveCodingManager()
	: LastCompileTimestamp(FDateTime(0))
	, LastCompileResult(ELiveCodingCompileResult::NotStarted)
	, bHasCompileResult(false)
	, bCompileInProgress(false)
{
}

FSlateAgentBridgeLiveCodingManager::~FSlateAgentBridgeLiveCodingManager()
{
	Shutdown();
}

void FSlateAgentBridgeLiveCodingManager::Initialize()
{
	if (!LogCapture.IsValid())
	{
		LogCapture = MakeUnique<FSlateAgentBridgeLiveCodingLogCapture>();
		if (GLog)
		{
			GLog->AddOutputDevice(LogCapture.Get());
		}
	}

	{
		FScopeLock LogLock(&LogMutex);
		LastCompileLogEntries.Reset();
		LastCompileTimestamp = FDateTime(0);
		LastCompileResult = ELiveCodingCompileResult::NotStarted;
		bHasCompileResult = false;
		LastErrorMessage.Reset();
	}

	bCompileInProgress.Store(false);
}

void FSlateAgentBridgeLiveCodingManager::Shutdown()
{
	if (LogCapture.IsValid())
	{
		if (GLog)
		{
			GLog->RemoveOutputDevice(LogCapture.Get());
		}
		LogCapture.Reset();
	}
}

bool FSlateAgentBridgeLiveCodingManager::TryBeginCompile(FString& OutErrorMessage)
{
	bool bExpected = false;
	if (!bCompileInProgress.CompareExchange(bExpected, true))
	{
		OutErrorMessage = TEXT("A Live Coding compile is already in progress.");
		return false;
	}

	if (!EnsureCaptureAvailable(OutErrorMessage))
	{
		bCompileInProgress.Store(false);
		return false;
	}

	{
		FScopeLock LogLock(&LogMutex);
		LastCompileTimestamp = FDateTime::UtcNow();
		LastCompileResult = ELiveCodingCompileResult::InProgress;
		LastErrorMessage.Reset();
	}

	UE_LOG(LogSlateAgentBridge, Display, TEXT("Live Coding compile request queued."));

	return true;
}

void FSlateAgentBridgeLiveCodingManager::ExecuteCompileOnGameThread()
{
	FString ErrorMessage;
	ELiveCodingCompileResult CompileResult = ELiveCodingCompileResult::NotStarted;

	if (!EnsureCaptureAvailable(ErrorMessage))
	{
		FinalizeCompileWithError(ErrorMessage, ELiveCodingCompileResult::Failure);
		return;
	}

	ILiveCodingModule* LiveCodingModule = nullptr;
	if (!EnsureLiveCodingAvailable(ErrorMessage, LiveCodingModule))
	{
		FinalizeCompileWithError(ErrorMessage, ELiveCodingCompileResult::Failure);
		return;
	}

	if (!LiveCodingModule->IsEnabledForSession())
	{
		LiveCodingModule->EnableForSession(true);
	}

	if (!LiveCodingModule->HasStarted())
	{
		LiveCodingModule->EnableForSession(true);
	}

	if (LiveCodingModule->IsCompiling())
	{
		FinalizeCompileWithError(TEXT("A Live Coding compile is already in progress."), ELiveCodingCompileResult::CompileStillActive);
		return;
	}

	UE_LOG(LogSlateAgentBridge, Display, TEXT("Live Coding compile started via HTTP endpoint."));

	LogCapture->StartCapture();
	const bool bCompileRequestAccepted = LiveCodingModule->Compile(ELiveCodingCompileFlags::WaitForCompletion, &CompileResult);
	TArray<FSlateAgentBridgeLogEntry> CapturedEntries = LogCapture->StopCapture();

	if (!bCompileRequestAccepted)
	{
		FinalizeCompileWithError(TEXT("Live Coding compile request was rejected."), ELiveCodingCompileResult::Failure);
		return;
	}

	FinalizeCompile(MoveTemp(CapturedEntries), CompileResult, FString());
}

void FSlateAgentBridgeLiveCodingManager::GetLastCompileSnapshot(TArray<FSlateAgentBridgeLogEntry>& OutEntries, FDateTime& OutTimestamp, ELiveCodingCompileResult& OutResult, bool& bOutHasResult, FString& OutErrorMessage, bool& bOutIsInProgress) const
{
	FScopeLock LogLock(&LogMutex);
	OutEntries = LastCompileLogEntries;
	OutTimestamp = LastCompileTimestamp;
	OutResult = LastCompileResult;
	bOutHasResult = bHasCompileResult;
	OutErrorMessage = LastErrorMessage;
	bOutIsInProgress = bCompileInProgress.Load();
}

FString FSlateAgentBridgeLiveCodingManager::CompileResultToString(ELiveCodingCompileResult CompileResult)
{
	switch (CompileResult)
	{
	case ELiveCodingCompileResult::Success:
		return TEXT("Success");
	case ELiveCodingCompileResult::NoChanges:
		return TEXT("NoChanges");
	case ELiveCodingCompileResult::InProgress:
		return TEXT("InProgress");
	case ELiveCodingCompileResult::CompileStillActive:
		return TEXT("CompileStillActive");
	case ELiveCodingCompileResult::NotStarted:
		return TEXT("NotStarted");
	case ELiveCodingCompileResult::Failure:
		return TEXT("Failure");
	case ELiveCodingCompileResult::Cancelled:
		return TEXT("Cancelled");
	default:
		return TEXT("Unknown");
	}
}

bool FSlateAgentBridgeLiveCodingManager::EnsureCaptureAvailable(FString& OutErrorMessage)
{
	if (!LogCapture.IsValid())
	{
		OutErrorMessage = TEXT("Live coding log capture is not available.");
		UE_LOG(LogSlateAgentBridge, Error, TEXT("%s"), *OutErrorMessage);
		return false;
	}

	return true;
}

bool FSlateAgentBridgeLiveCodingManager::EnsureLiveCodingAvailable(FString& OutErrorMessage, ILiveCodingModule*& OutModule) const
{
	OutModule = FModuleManager::LoadModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!OutModule)
	{
		OutErrorMessage = TEXT("Live Coding module is unavailable. Enable Live Coding in the editor first.");
		UE_LOG(LogSlateAgentBridge, Error, TEXT("%s"), *OutErrorMessage);
		return false;
	}

	if (!OutModule->CanEnableForSession())
	{
		OutErrorMessage = OutModule->GetEnableErrorText().ToString();
		UE_LOG(LogSlateAgentBridge, Error, TEXT("Live Coding cannot be enabled: %s"), *OutErrorMessage);
		return false;
	}

	return true;
}

void FSlateAgentBridgeLiveCodingManager::FinalizeCompile(TArray<FSlateAgentBridgeLogEntry>&& CapturedEntries, ELiveCodingCompileResult Result, const FString& ErrorMessage)
{
	{
		FScopeLock LogLock(&LogMutex);
		LastCompileLogEntries = MoveTemp(CapturedEntries);
		LastCompileTimestamp = FDateTime::UtcNow();
		LastCompileResult = Result;
		LastErrorMessage = ErrorMessage;
		bHasCompileResult = true;
	}

	bCompileInProgress.Store(false);

	switch (Result)
	{
	case ELiveCodingCompileResult::Success:
		UE_LOG(LogSlateAgentBridge, Display, TEXT("Live Coding compile completed with changes."));
		break;
	case ELiveCodingCompileResult::NoChanges:
		UE_LOG(LogSlateAgentBridge, Display, TEXT("Live Coding compile completed with no changes."));
		break;
	case ELiveCodingCompileResult::Failure:
		UE_LOG(LogSlateAgentBridge, Error, TEXT("Live Coding compile failed. See log for details."));
		break;
	case ELiveCodingCompileResult::Cancelled:
		UE_LOG(LogSlateAgentBridge, Warning, TEXT("Live Coding compile was cancelled."));
		break;
	default:
		break;
	}
}

void FSlateAgentBridgeLiveCodingManager::FinalizeCompileWithError(const FString& ErrorMessage, ELiveCodingCompileResult Result)
{
	TArray<FSlateAgentBridgeLogEntry> EmptyEntries;
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogSlateAgentBridge, Error, TEXT("%s"), *ErrorMessage);
	}
	FinalizeCompile(MoveTemp(EmptyEntries), Result, ErrorMessage);
}
