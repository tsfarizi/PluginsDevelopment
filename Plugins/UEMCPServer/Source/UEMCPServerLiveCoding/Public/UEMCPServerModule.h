// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"

class FUEMCPServerLiveCodingManager;
class FUEMCPServerMcpServer;

/**
 * This is the module definition for the editor mode. You can implement custom functionality
 * as your plugin module starts up and shuts down. See IModuleInterface for more extensibility options.
 */
class FUEMCPServerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool StartMcpServer();
	void StopMcpServer();

private:
	TUniquePtr<FUEMCPServerMcpServer> McpServer;
	TUniquePtr<FUEMCPServerLiveCodingManager> LiveCodingManager;
	uint32 McpServerPort = 8133;
	FString McpBindAddress;
};
