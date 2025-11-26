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
