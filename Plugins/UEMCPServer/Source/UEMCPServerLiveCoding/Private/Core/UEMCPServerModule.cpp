#include "UEMCPServerModule.h"

#include "Mcp/UEMCPServerMcpServer.h"
#include "UEMCPServerEditorModeCommands.h"
#include "LiveCoding/UEMCPServerLiveCodingManager.h"
#include "UEMCPServerLiveCodingTypes.h"
#include "UEMCPServerLog.h"

#include "Math/NumericLimits.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogUEMCPServer);

namespace UEMCPServer
{
    static constexpr uint32 DefaultPort = 8133;
    static constexpr const TCHAR* ConfigSection = TEXT("/Script/UEMCPServer.UEMCPServerSettings");
    static constexpr const TCHAR* ConfigPortKey = TEXT("LiveCodingHttpPort");
    static constexpr const TCHAR* LegacyConfigPortKey = TEXT("LiveCodingWebSocketPort");
    static constexpr const TCHAR* ConfigBindKey = TEXT("LiveCodingHttpBindAddress");
    static constexpr const TCHAR* LegacyConfigBindKey = TEXT("LiveCodingWebSocketBindAddress");
}

void FUEMCPServerModule::StartupModule()
{
    McpServerPort = UEMCPServer::DefaultPort;
    McpBindAddress = TEXT("127.0.0.1");

    if (GConfig)
    {
        int32 ConfiguredPort = 0;
        if (GConfig->GetInt(UEMCPServer::ConfigSection, UEMCPServer::ConfigPortKey, ConfiguredPort, GEditorPerProjectIni)
            && ConfiguredPort > 0 && ConfiguredPort <= TNumericLimits<uint16>::Max())
        {
            McpServerPort = static_cast<uint32>(ConfiguredPort);
        }
        else if (GConfig->GetInt(UEMCPServer::ConfigSection, UEMCPServer::LegacyConfigPortKey, ConfiguredPort, GEditorPerProjectIni)
            && ConfiguredPort > 0 && ConfiguredPort <= TNumericLimits<uint16>::Max())
        {
            McpServerPort = static_cast<uint32>(ConfiguredPort);
            UE_LOG(LogUEMCPServer, Verbose, TEXT("Using legacy configuration key LiveCodingWebSocketPort (%d) for MCP server port."), ConfiguredPort);
        }

        FString ConfiguredBind;
        if (GConfig->GetString(UEMCPServer::ConfigSection, UEMCPServer::ConfigBindKey, ConfiguredBind, GEditorPerProjectIni)
            && !ConfiguredBind.IsEmpty())
        {
            McpBindAddress = ConfiguredBind;
        }
        else if (GConfig->GetString(UEMCPServer::ConfigSection, UEMCPServer::LegacyConfigBindKey, ConfiguredBind, GEditorPerProjectIni)
            && !ConfiguredBind.IsEmpty())
        {
            McpBindAddress = ConfiguredBind;
            UE_LOG(LogUEMCPServer, Verbose, TEXT("Using legacy configuration key LiveCodingWebSocketBindAddress (%s) for MCP server bind address."), *ConfiguredBind);
        }
    }

    LiveCodingManager = MakeUnique<FUEMCPServerLiveCodingManager>();
    LiveCodingManager->Initialize();

    if (StartMcpServer())
    {
        UE_LOG(LogUEMCPServer, Display, TEXT("UEMCPServer MCP server listening on http://%s:%u/mcp"),
            McpBindAddress.IsEmpty() ? TEXT("0.0.0.0") : *McpBindAddress,
            McpServerPort);
    }
    else
    {
        UE_LOG(LogUEMCPServer, Error, TEXT("Failed to start UEMCPServer MCP server on %s:%u."),
            McpBindAddress.IsEmpty() ? TEXT("0.0.0.0") : *McpBindAddress,
            McpServerPort);
    }

    FUEMCPServerEditorModeCommands::Register();
}

void FUEMCPServerModule::ShutdownModule()
{
    FUEMCPServerEditorModeCommands::Unregister();

    StopMcpServer();

    if (LiveCodingManager)
    {
        LiveCodingManager->Shutdown();
        LiveCodingManager.Reset();
    }

    UE_LOG(LogUEMCPServer, Display, TEXT("UEMCPServer module shut down."));
}

bool FUEMCPServerModule::StartMcpServer()
{
    if (!LiveCodingManager.IsValid())
    {
        return false;
    }

    if (McpServer.IsValid())
    {
        return true;
    }

    McpServer = MakeUnique<FUEMCPServerMcpServer>(*LiveCodingManager, McpServerPort, McpBindAddress);
    if (!McpServer->Start())
    {
        McpServer.Reset();
        return false;
    }

    return true;
}

void FUEMCPServerModule::StopMcpServer()
{
    if (McpServer)
    {
        McpServer->Stop();
        McpServer.Reset();
    }
}

IMPLEMENT_MODULE(FUEMCPServerModule, UEMCPServerLiveCoding)
