#include "SlateAgentBridgeModule.h"

#include "Mcp/SlateAgentBridgeMcpServer.h"
#include "SlateAgentBridgeEditorModeCommands.h"
#include "LiveCoding/SlateAgentBridgeLiveCodingManager.h"
#include "SlateAgentBridgeLiveCodingTypes.h"
#include "SlateAgentBridgeLog.h"

#include "Math/NumericLimits.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogSlateAgentBridge);

namespace SlateAgentBridge
{
    static constexpr uint32 DefaultPort = 8133;
    static constexpr const TCHAR* ConfigSection = TEXT("/Script/SlateAgentBridge.SlateAgentBridgeSettings");
    static constexpr const TCHAR* ConfigPortKey = TEXT("LiveCodingHttpPort");
    static constexpr const TCHAR* LegacyConfigPortKey = TEXT("LiveCodingWebSocketPort");
    static constexpr const TCHAR* ConfigBindKey = TEXT("LiveCodingHttpBindAddress");
    static constexpr const TCHAR* LegacyConfigBindKey = TEXT("LiveCodingWebSocketBindAddress");
}

void FSlateAgentBridgeModule::StartupModule()
{
    McpServerPort = SlateAgentBridge::DefaultPort;
    McpBindAddress = TEXT("127.0.0.1");

    if (GConfig)
    {
        int32 ConfiguredPort = 0;
        if (GConfig->GetInt(SlateAgentBridge::ConfigSection, SlateAgentBridge::ConfigPortKey, ConfiguredPort, GEditorPerProjectIni)
            && ConfiguredPort > 0 && ConfiguredPort <= TNumericLimits<uint16>::Max())
        {
            McpServerPort = static_cast<uint32>(ConfiguredPort);
        }
        else if (GConfig->GetInt(SlateAgentBridge::ConfigSection, SlateAgentBridge::LegacyConfigPortKey, ConfiguredPort, GEditorPerProjectIni)
            && ConfiguredPort > 0 && ConfiguredPort <= TNumericLimits<uint16>::Max())
        {
            McpServerPort = static_cast<uint32>(ConfiguredPort);
            UE_LOG(LogSlateAgentBridge, Verbose, TEXT("Using legacy configuration key LiveCodingWebSocketPort (%d) for MCP server port."), ConfiguredPort);
        }

        FString ConfiguredBind;
        if (GConfig->GetString(SlateAgentBridge::ConfigSection, SlateAgentBridge::ConfigBindKey, ConfiguredBind, GEditorPerProjectIni)
            && !ConfiguredBind.IsEmpty())
        {
            McpBindAddress = ConfiguredBind;
        }
        else if (GConfig->GetString(SlateAgentBridge::ConfigSection, SlateAgentBridge::LegacyConfigBindKey, ConfiguredBind, GEditorPerProjectIni)
            && !ConfiguredBind.IsEmpty())
        {
            McpBindAddress = ConfiguredBind;
            UE_LOG(LogSlateAgentBridge, Verbose, TEXT("Using legacy configuration key LiveCodingWebSocketBindAddress (%s) for MCP server bind address."), *ConfiguredBind);
        }
    }

    LiveCodingManager = MakeUnique<FSlateAgentBridgeLiveCodingManager>();
    LiveCodingManager->Initialize();

    if (StartMcpServer())
    {
        UE_LOG(LogSlateAgentBridge, Display, TEXT("SlateAgentBridge MCP server listening on http://%s:%u/mcp"),
            McpBindAddress.IsEmpty() ? TEXT("0.0.0.0") : *McpBindAddress,
            McpServerPort);
    }
    else
    {
        UE_LOG(LogSlateAgentBridge, Error, TEXT("Failed to start SlateAgentBridge MCP server on %s:%u."),
            McpBindAddress.IsEmpty() ? TEXT("0.0.0.0") : *McpBindAddress,
            McpServerPort);
    }

    FSlateAgentBridgeEditorModeCommands::Register();
}

void FSlateAgentBridgeModule::ShutdownModule()
{
    FSlateAgentBridgeEditorModeCommands::Unregister();

    StopMcpServer();

    if (LiveCodingManager)
    {
        LiveCodingManager->Shutdown();
        LiveCodingManager.Reset();
    }

    UE_LOG(LogSlateAgentBridge, Display, TEXT("SlateAgentBridge module shut down."));
}

bool FSlateAgentBridgeModule::StartMcpServer()
{
    if (!LiveCodingManager.IsValid())
    {
        return false;
    }

    if (McpServer.IsValid())
    {
        return true;
    }

    McpServer = MakeUnique<FSlateAgentBridgeMcpServer>(*LiveCodingManager, McpServerPort, McpBindAddress);
    if (!McpServer->Start())
    {
        McpServer.Reset();
        return false;
    }

    return true;
}

void FSlateAgentBridgeModule::StopMcpServer()
{
    if (McpServer)
    {
        McpServer->Stop();
        McpServer.Reset();
    }
}

IMPLEMENT_MODULE(FSlateAgentBridgeModule, SlateAgentBridge)
