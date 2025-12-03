// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"

struct FUEMCPServerLogEntry
{
	FString Message;
	FString Category;
	FString Verbosity;
	FDateTime Timestamp;
};

#include "ILiveCodingModule.h"

namespace UEMCPServer
{
	inline FString CompileResultToString(ELiveCodingCompileResult CompileResult)
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
}
