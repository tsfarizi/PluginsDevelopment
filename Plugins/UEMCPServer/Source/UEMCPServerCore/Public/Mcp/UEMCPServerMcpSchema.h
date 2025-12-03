#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;

class UEMCPSERVERCORE_API UEMCPServerMcpSchema
{
public:
	static TSharedRef<FJsonObject> BuildToolInputSchema(bool bIncludeWaitFlag);
	static TSharedRef<FJsonObject> BuildLiveCodingOutputSchema();
	static void PopulateToolsList(TArray<TSharedPtr<FJsonValue>>& OutTools);
};
