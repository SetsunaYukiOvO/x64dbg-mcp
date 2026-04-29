#include "core/Logger.h"
#include "core/ConfigManager.h"
#include "core/MethodDispatcher.h"
#include "core/PermissionChecker.h"
#include "core/MCPToolRegistry.h"
#include "core/MCPResourceRegistry.h"
#include "core/MCPPromptRegistry.h"
#include "core/X64DBGBridge.h"
#include "business/DebugController.h"
#include "handlers/DebugHandler.h"
#include "handlers/RegisterHandler.h"
#include "handlers/MemoryHandler.h"
#include "handlers/BreakpointHandler.h"
#include "handlers/DisassemblyHandler.h"
#include "handlers/StackHandler.h"
#include "handlers/ThreadHandler.h"
#include "handlers/ModuleHandler.h"
#include "handlers/EventCallbackHandler.h"
#include "handlers/ScriptHandler.h"
#include "handlers/ContextHandler.h"
#include "handlers/DumpHandler.h"
#include "handlers/ExpressionHandler.h"
#include "handlers/AnalysisHandler.h"
#include "handlers/AssemblerHandler.h"
#include "handlers/BookmarkHandler.h"
#include "handlers/PatchHandler.h"
#include "utils/StringUtils.h"
#include "communication/MCPHttpServer.h"
#include "ui/ConfigEditor.h"
#include <windows.h>
#include <filesystem>
#include <fstream>

// 鎻掍欢鐗堟湰淇℃�?
#define PLUGIN_VERSION "1.0.6"

// 鍙�?CMake 瑕嗙洊锛歅LUGIN_DISPLAY_NAME, PLUGIN_DIR_NAME
#ifndef PLUGIN_DISPLAY_NAME
#define PLUGIN_DISPLAY_NAME "x64dbg MCP Server"
#endif

#ifndef PLUGIN_DIR_NAME
#define PLUGIN_DIR_NAME "x64dbg-mcp"
#endif

#ifndef PLUGIN_NAME
#define PLUGIN_NAME PLUGIN_DISPLAY_NAME
#endif

// 鍏ㄥ眬鍙橀�?
static int g_pluginHandle = 0;
static int g_menuHandle = 0;
static std::unique_ptr<MCP::MCPHttpServer> g_mcpHttpServer;
static HMODULE g_hModule = NULL;  // 鎻掍欢妯″潡鍙ユ�?
static constexpr const char* kAutoStartConfigKey = "features.auto_start_mcp_on_plugin_load";

// 鑿滃崟鍛戒护ID - 浣跨敤鍞竴鐨処D�?
enum MenuCommands {
    MENU_START_MCP_HTTP = 1,
    MENU_STOP_MCP_HTTP = 2,
    MENU_EDIT_CONFIG = 3,
    MENU_SHOW_CONFIG = 4,
    MENU_AUTO_START_MCP = 5,
    MENU_ABOUT = 6
};

namespace MCP {

static std::string GetPluginBaseDirectory() {
    char modulePath[MAX_PATH] = {0};
    if (g_hModule && GetModuleFileNameA(g_hModule, modulePath, MAX_PATH) > 0) {
        std::filesystem::path dllPath(modulePath);
        return dllPath.parent_path().string();
    }
    return std::string();
}

static std::string GetActiveConfigPath() {
    auto& config = ConfigManager::Instance();
    std::string configPath = config.GetConfigPath();

    if (!configPath.empty()) {
        std::filesystem::path loadedPath(configPath);
        if (loadedPath.is_relative()) {
            const std::string baseDir = GetPluginBaseDirectory();
            if (!baseDir.empty()) {
                configPath = (std::filesystem::path(baseDir) / loadedPath).lexically_normal().string();
            }
        }
        return configPath;
    }

    const std::string baseDir = GetPluginBaseDirectory();
    if (!baseDir.empty()) {
        return (std::filesystem::path(baseDir) / PLUGIN_DIR_NAME / "config.json").string();
    }

    return std::string(PLUGIN_DIR_NAME) + "\\config.json";
}

static bool StartMcpHttpServer(bool reloadConfig) {
    if (!g_mcpHttpServer) {
        g_mcpHttpServer = std::make_unique<MCP::MCPHttpServer>();
    }

    if (g_mcpHttpServer->IsRunning()) {
        _plugin_logputs("[MCP] HTTP Server is already running");
        return true;
    }

    auto& config = ConfigManager::Instance();
    const std::string configPath = GetActiveConfigPath();

    if (reloadConfig && !config.Load(configPath)) {
        _plugin_logprintf("[MCP] Failed to reload config from: %s\n", configPath.c_str());
    }

    MCP::PermissionChecker::Instance().Initialize();

    std::string address = config.GetServerAddress();
    int port = static_cast<int>(config.GetServerPort());

    if (g_mcpHttpServer->Start(address, port)) {
        _plugin_logprintf("[MCP] HTTP Server started on http://%s:%d\n", address.c_str(), port);
        _plugin_logputs("[MCP] Configure VSCode/Claude to connect via SSE");
        return true;
    }

    _plugin_logputs("[MCP] Failed to start HTTP Server");
    return false;
}

static void UpdateAutoStartMenuCheck() {
    const bool enabled = ConfigManager::Instance().Get<bool>(kAutoStartConfigKey, false);
    _plugin_menuentrysetchecked(g_pluginHandle, MENU_AUTO_START_MCP, enabled);
}


/**
 * @brief x64dbg 鍥炶�? 鍒濆鍖栬皟�?
 */
static void CB_InitDebug(CBTYPE cbType, void* callbackInfo) {
    Logger::Info("Debug session started");
}

/**
 * @brief x64dbg 鍥炶�? 鍋滄璋冭瘯
 */
static void CB_StopDebug(CBTYPE cbType, void* callbackInfo) {
    Logger::Info("Debug session stopped");
}

/**
 * @brief x64dbg 鍥炶�? 鏂偣鍛戒腑
 */
static void CB_Breakpoint(CBTYPE cbType, void* callbackInfo) {
    uint64_t address = 0;

    const auto* info = static_cast<PLUG_CB_BREAKPOINT*>(callbackInfo);
    if (info && info->breakpoint) {
        address = static_cast<uint64_t>(info->breakpoint->addr);
    }

    if (address == 0) {
        // Current x64dbg SDK exposes DbgValFromString as a direct value return.
        const duint cip = DbgValFromString("cip");
        if (cip != 0) {
            address = static_cast<uint64_t>(cip);
        }
    }

    EventCallbackHandler::OnBreakpoint(address);
}

/**
 * @brief x64dbg 鍥炶�? 寮傚�?
 */
static void CB_Exception(CBTYPE cbType, void* callbackInfo) {
    uint32_t code = 0;
    uint64_t address = 0;
    bool firstChance = true;

    const auto* info = static_cast<PLUG_CB_EXCEPTION*>(callbackInfo);
    if (info && info->Exception) {
        const EXCEPTION_RECORD& record = info->Exception->ExceptionRecord;
        code = record.ExceptionCode;
        address = reinterpret_cast<uint64_t>(record.ExceptionAddress);
        firstChance = info->Exception->dwFirstChance != 0;
    }

    EventCallbackHandler::OnException(code, address, firstChance);
}

/**
 * @brief x64dbg 鍥炶�? 妯″潡鍔犺�?
 */
static void CB_LoadDll(CBTYPE cbType, void* callbackInfo) {
    const auto* info = static_cast<PLUG_CB_LOADDLL*>(callbackInfo);

    uint64_t base = 0;
    uint64_t size = 0;
    const char* moduleName = "";
    char resolvedName[MAX_MODULE_SIZE] = {};

    if (info) {
        if (info->modname) {
            moduleName = info->modname;
        }

        if (info->modInfo) {
            base = static_cast<uint64_t>(info->modInfo->BaseOfImage);
            size = static_cast<uint64_t>(info->modInfo->ImageSize);
        }

        if (base == 0 && info->LoadDll && info->LoadDll->lpBaseOfDll) {
            base = reinterpret_cast<uint64_t>(info->LoadDll->lpBaseOfDll);
        }
    }

    if (moduleName[0] == '\0' && base != 0 &&
        Script::Module::NameFromAddr(static_cast<duint>(base), resolvedName)) {
        moduleName = resolvedName;
    }

    EventCallbackHandler::OnModuleLoad(moduleName, base, size);
}

/**
 * @brief x64dbg 鍥炶�? 妯″潡鍗歌�?
 */
static void CB_UnloadDll(CBTYPE cbType, void* callbackInfo) {
    uint64_t base = 0;
    const char* moduleName = "";
    char resolvedName[MAX_MODULE_SIZE] = {};

    const auto* info = static_cast<PLUG_CB_UNLOADDLL*>(callbackInfo);
    if (info && info->UnloadDll && info->UnloadDll->lpBaseOfDll) {
        base = reinterpret_cast<uint64_t>(info->UnloadDll->lpBaseOfDll);
    }

    if (base != 0 &&
        Script::Module::NameFromAddr(static_cast<duint>(base), resolvedName)) {
        moduleName = resolvedName;
    }

    EventCallbackHandler::OnModuleUnload(moduleName, base);
}

/**
 * @brief x64dbg 鍥炶�? 杩涚▼鍒涘缓
 */
static void CB_CreateProcess(CBTYPE cbType, void* callbackInfo) {
    // Cache the debuggee path so debug.restart / debug.init (with no path)
    // can re-launch the target after the process exits or crashes, even if
    // debugging was started via x64dbg's GUI.
    char path[MAX_PATH] = {};
    if (Script::Module::GetMainModulePath(path) && path[0] != '\0') {
        DebugController::Instance().NotifyDebugSessionStarted(path);
    }

    EventCallbackHandler::OnCreateProcess();
}

/**
 * @brief x64dbg 鍥炶�? 杩涚▼閫€鍑?
 */
static void CB_ExitProcess(CBTYPE cbType, void* callbackInfo) {
    EventCallbackHandler::OnExitProcess();
}

/**
 * @brief x64dbg 鍥炶�? 鑿滃崟椤圭偣�?
 */
static void CB_MenuEntry(CBTYPE cbType, void* callbackInfo) {
    auto* info = static_cast<PLUG_CB_MENUENTRY*>(callbackInfo);
    
    try {
        if (info->hEntry == MENU_EDIT_CONFIG) {
            // 缂栬緫閰嶇疆
            _plugin_logputs("[MCP] Opening config editor...");
            
            auto& config = ConfigManager::Instance();
            
            // 鏋勫缓閰嶇疆鏂囦欢璺緞
            std::string configPath = GetActiveConfigPath();
            _plugin_logprintf("[MCP] Config path: %s\n", configPath.c_str());
            
            // 鑾峰彇x64dbg涓荤獥鍙ｅ彞�?
            HWND hwndDlg = GuiGetWindowHandle();
            
            if (MCP::ConfigEditor::Show(g_hModule, hwndDlg, configPath)) {
                _plugin_logputs("[MCP] Configuration updated");
                // 閲嶆柊鍔犺浇閰嶇�?
                config.Load(configPath);
                MCP::PermissionChecker::Instance().Initialize();
                UpdateAutoStartMenuCheck();
                
                // 濡傛灉鏈嶅姟鍣ㄦ鍦ㄨ繍�?鎻愮ず闇€瑕侀噸鍚?
                if (g_mcpHttpServer && g_mcpHttpServer->IsRunning()) {
                    MessageBoxA(hwndDlg, 
                        "Configuration saved. Please restart MCP HTTP Server for changes to take effect.", 
                        "Config Updated", 
                        MB_OK | MB_ICONINFORMATION);
                }
            } else {
                _plugin_logputs("[MCP] Config editor cancelled or failed");
            }
        }
        else if (info->hEntry == MENU_SHOW_CONFIG) {
            // 鏄剧ず閰嶇疆
            auto& config = ConfigManager::Instance();
            
            _plugin_logputs("[MCP] Current Configuration:");
            _plugin_logprintf("  Address: %s\n", config.Get<std::string>("server.address", "127.0.0.1").c_str());
            _plugin_logprintf("  Port: %d\n", config.Get<int>("server.port", 3000));
            _plugin_logprintf("  Auto Start MCP on Load: %s\n", config.Get<bool>(kAutoStartConfigKey, false) ? "ON" : "OFF");
        }
        else if (info->hEntry == MENU_START_MCP_HTTP) {
            StartMcpHttpServer(true);
        }
        else if (info->hEntry == MENU_STOP_MCP_HTTP) {
            // 鍋滄�?MCP HTTP 鏈嶅姟鍣?
            if (!g_mcpHttpServer || !g_mcpHttpServer->IsRunning()) {
                _plugin_logputs("[MCP] HTTP Server is not running");
                return;
            }
            
            g_mcpHttpServer->Stop();
            _plugin_logputs("[MCP] HTTP Server stopped");
        }
        else if (info->hEntry == MENU_AUTO_START_MCP) {
            auto& config = ConfigManager::Instance();
            const std::string configPath = GetActiveConfigPath();
            const bool currentValue = config.Get<bool>(kAutoStartConfigKey, false);
            const bool newValue = !currentValue;

            config.Set<bool>(kAutoStartConfigKey, newValue);
            if (config.Save(configPath)) {
                _plugin_menuentrysetchecked(g_pluginHandle, MENU_AUTO_START_MCP, newValue);
                _plugin_logprintf("[MCP] Auto-start MCP on plugin load: %s\n", newValue ? "ON" : "OFF");
                _plugin_logprintf("[MCP] Auto-start config saved to: %s\n", configPath.c_str());
            } else {
                config.Set<bool>(kAutoStartConfigKey, currentValue);
                _plugin_logputs("[MCP] Failed to save auto-start option to config");
                _plugin_logprintf("[MCP] Save target path: %s\n", configPath.c_str());
            }
        }
        else if (info->hEntry == MENU_ABOUT) {
            // 鍏充�?
            char aboutMsg[256];
            sprintf_s(aboutMsg, "[MCP] %s Plugin v%s", PLUGIN_DISPLAY_NAME, PLUGIN_VERSION);
            _plugin_logputs(aboutMsg);
            _plugin_logputs("[MCP] Provides JSON-RPC debugging interface");
            _plugin_logputs("[MCP] https://github.com/SetsunaYukiOvO/x64dbg-mcp");
        }
    } catch (const std::exception& e) {
        _plugin_logprintf("[MCP] Menu callback error: %s\n", e.what());
    }
}

} // namespace MCP

/**
 * @brief 娉ㄥ唽鎵€鏈?JSON-RPC 鏂规�?
 */
static void RegisterAllMethods() {
    // Wire DbgEval into the address parser so all handlers can resolve
    // symbol names, register names, and expressions (e.g. "kernel32.CreateFileW", "RIP").
    MCP::StringUtils::SetAddressResolver([](const std::string& expr) -> std::optional<uint64_t> {
        bool success = false;
        duint result = DbgEval(expr.c_str(), &success);
        if (success) return static_cast<uint64_t>(result);
        return std::nullopt;
    });

    // 娉ㄥ�?MCP 宸ュ叿瀹氫�?
    MCP::MCPToolRegistry::Instance().RegisterDefaultTools();
    
    // 娉ㄥ�?MCP 璧勬簮鍜屾彁绀鸿瘝
    MCP::MCPResourceRegistry::Instance().RegisterDefaultResources();
    MCP::MCPPromptRegistry::Instance().RegisterDefaultPrompts();
    
    // 娉ㄥ�?JSON-RPC 鏂规硶澶勭悊�?
    MCP::DebugHandler::RegisterMethods();
    MCP::RegisterHandler::RegisterMethods();
    MCP::MemoryHandler::RegisterMethods();
    MCP::BreakpointHandler::RegisterMethods();
    MCP::DisassemblyHandler::RegisterMethods();
    MCP::SymbolHandler::RegisterMethods();
    MCP::StackHandler::RegisterMethods();
    MCP::ThreadHandler::RegisterMethods();
    MCP::ModuleHandler::RegisterMethods();
    MCP::DumpHandler::RegisterMethods();
    MCP::ExpressionHandler::RegisterMethods();
    MCP::AnalysisHandler::RegisterMethods();
    MCP::AssemblerHandler::RegisterMethods();
    MCP::BookmarkHandler::RegisterMethods();
    MCP::PatchHandler::RegisterMethods();
    
    // Register script execution methods
    auto& dispatcher = MCP::MethodDispatcher::Instance();
    dispatcher.RegisterMethod("script.execute", [](const json& params) -> json {
        return ScriptHandler::execute(params);
    });
    dispatcher.RegisterMethod("script.execute_batch", [](const json& params) -> json {
        return ScriptHandler::executeBatch(params);
    });
    dispatcher.RegisterMethod("script.get_last_result", [](const json& params) -> json {
        return ScriptHandler::getLastResult(params);
    });
    
    // Register context snapshot methods
    dispatcher.RegisterMethod("context.get_snapshot", [](const json& params) -> json {
        return ContextHandler::getSnapshot(params);
    });
    dispatcher.RegisterMethod("context.get_basic", [](const json& params) -> json {
        return ContextHandler::getBasicContext(params);
    });
    dispatcher.RegisterMethod("context.compare_snapshots", [](const json& params) -> json {
        return ContextHandler::compareSnapshots(params);
    });
    
    MCP::Logger::Info("All JSON-RPC methods registered");
}

/**
 * @brief 娉ㄥ唽鎵€鏈夊洖�?
 */
static void RegisterCallbacks() {
    _plugin_registercallback(g_pluginHandle, CB_INITDEBUG, MCP::CB_InitDebug);
    _plugin_registercallback(g_pluginHandle, CB_STOPDEBUG, MCP::CB_StopDebug);
    _plugin_registercallback(g_pluginHandle, CB_BREAKPOINT, MCP::CB_Breakpoint);
    _plugin_registercallback(g_pluginHandle, CB_EXCEPTION, MCP::CB_Exception);
    _plugin_registercallback(g_pluginHandle, CB_CREATEPROCESS, MCP::CB_CreateProcess);
    _plugin_registercallback(g_pluginHandle, CB_EXITPROCESS, MCP::CB_ExitProcess);
    _plugin_registercallback(g_pluginHandle, CB_LOADDLL, MCP::CB_LoadDll);
    _plugin_registercallback(g_pluginHandle, CB_UNLOADDLL, MCP::CB_UnloadDll);
    _plugin_registercallback(g_pluginHandle, CB_MENUENTRY, MCP::CB_MenuEntry);  // 娉ㄥ唽鑿滃崟鍥炶�?
    
    MCP::Logger::Info("All callbacks registered");
}

// ==================== x64dbg 鎻掍欢瀵煎嚭鍑芥暟 ====================

/**
 * @brief 鎻掍欢鍒濆�?
 * @return true琛ㄧず鍒濆鍖栨垚鍔燂紝false琛ㄧず澶辫触
 */
extern "C" __declspec(dllexport) bool pluginit(PLUG_INITSTRUCT* initStruct) {
    // �?蹇呴』濉啓鎻掍欢淇℃伅锛屽惁鍒�?4dbg鏃犳硶璇嗗埆鎻掍�?
    initStruct->sdkVersion = PLUG_SDKVERSION;
    initStruct->pluginVersion = 1;  // 鎻掍欢鐗堟湰�?
    strcpy_s(initStruct->pluginName, PLUGIN_NAME);  // 鎻掍欢鍚嶇�?
    
    g_pluginHandle = initStruct->pluginHandle;
    
    try {
        // 鑾峰彇褰撳墠妯″潡璺緞锛岀敤浜庢棩蹇楁枃浠?
        char logPath[MAX_PATH] = {0};
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)&pluginit, &hModule)) {
            GetModuleFileNameA(hModule, logPath, MAX_PATH);
            // 鏇挎崲鏂囦欢鍚嶄负鏃ュ織鏂囦欢鍚?
            char* lastSlash = strrchr(logPath, '\\');
            if (lastSlash) {
                std::string logName = std::string(PLUGIN_DIR_NAME) + ".log";
                strcpy_s(lastSlash + 1, MAX_PATH - (lastSlash - logPath + 1), logName.c_str());
            }
        }
        
        // 濡傛灉鑾峰彇璺緞澶辫触锛屼娇鐢ㄩ粯璁よ矾寰?
        if (logPath[0] == 0) {
            std::string logName = std::string(PLUGIN_DIR_NAME) + ".log";
            strcpy_s(logPath, logName.c_str());
        }
        
        // 鍒濆鍖栨棩蹇楃郴缁?
        MCP::Logger::Initialize(logPath, MCP::LogLevel::Info, true);
        char initMsg[256];
        sprintf_s(initMsg, "%s Plugin v%s initializing...", PLUGIN_DISPLAY_NAME, PLUGIN_VERSION);
        MCP::Logger::Info(initMsg);
        _plugin_logprintf("[MCP] Log file: %s\n", logPath);
        
        _plugin_logprintf("[MCP] %s v%s\n", PLUGIN_DISPLAY_NAME, PLUGIN_VERSION);
        _plugin_logputs("[MCP] https://github.com/SetsunaYukiOvO/x64dbg-mcp");
        
        // 鍔犺浇閰嶇疆锛堝鏋滀笉瀛樺湪鍒欏垱寤洪粯璁ら厤缃�?
        try {
            auto& config = MCP::ConfigManager::Instance();
            std::string configPath = MCP::GetActiveConfigPath();
            
            // 妫€鏌ラ厤缃枃浠舵槸鍚﹀瓨鍦?
            if (!std::filesystem::exists(configPath)) {
                MCP::Logger::Warning("Config file not found, creating default config.json");
                _plugin_logputs("[MCP] Creating default config.json...");
                
                // 鍒涘缓鐩綍
                std::filesystem::path configFilePath(configPath);
                if (configFilePath.has_parent_path()) {
                    std::filesystem::create_directories(configFilePath.parent_path());
                }
                
                // 鍒涘缓榛樿閰嶇�?
                std::ofstream configFile(configPath);
                if (configFile.is_open()) {
                    configFile << R"({
  "version": "1.0.5",
  "server": {
    "address": "127.0.0.1",
    "port": 3000
  },
  "permissions": {
    "allow_memory_write": true,
    "allow_register_write": true,
    "allow_script_execution": true,
    "allow_breakpoint_modification": true,
    "allowed_methods": [
      "debug.*",
      "register.*",
      "memory.*",
      "breakpoint.*",
      "disasm.*",
      "disassembly.*",
      "module.*",
      "symbol.*",
      "thread.*",
      "stack.*",
      "comment.*",
      "script.*",
      "context.*",
      "dump.*"
    ]
  },
    "logging": {
        "enabled": true,
        "level": "info",
        "file": "plugin.log",
        "max_file_size_mb": 10,
        "console_output": true
    },
  "timeout": {
    "request_timeout_ms": 30000,
    "step_timeout_ms": 10000,
    "memory_read_timeout_ms": 5000
  },
  "features": {
    "enable_notifications": true,
    "enable_heartbeat": true,
    "heartbeat_interval_seconds": 30,
    "enable_batch_requests": true,
    "auto_start_mcp_on_plugin_load": false
  }
})";
                    configFile.close();
                    MCP::Logger::Info("Default config.json created successfully");
                    _plugin_logputs("[MCP] Default config.json created");
                } else {
                    MCP::Logger::Error("Failed to create config.json");
                    _plugin_logputs("[MCP] Failed to create config.json");
                }
            }
            
            config.Load(configPath);
            
            // 璁剧疆鏃ュ織绾у埆
            std::string logLevel = config.Get<std::string>("logging.level", "info");
            // 杞崲瀛楃涓插埌鏋氫�?
            MCP::LogLevel level = MCP::LogLevel::Info;
            if (logLevel == "trace") level = MCP::LogLevel::Trace;
            else if (logLevel == "debug") level = MCP::LogLevel::Debug;
            else if (logLevel == "warning") level = MCP::LogLevel::Warning;
            else if (logLevel == "error") level = MCP::LogLevel::Error;
            else if (logLevel == "critical") level = MCP::LogLevel::Critical;
            MCP::Logger::SetLevel(level);
        } catch (const std::exception& e) {
            MCP::Logger::Warning("Failed to load config: {}, using defaults", e.what());
        }
        
        // 鍒濆鍖栨潈闄愭鏌ュ櫒
        try {
            MCP::PermissionChecker::Instance().Initialize();
            MCP::Logger::Info("PermissionChecker initialized");
        } catch (const std::exception& e) {
            MCP::Logger::Error("Failed to initialize PermissionChecker: {}", e.what());
            _plugin_logprintf("[MCP] Failed to initialize PermissionChecker: %s\n", e.what());
        }
        
        // 娉ㄥ唽鎵€鏈?JSON-RPC 鏂规�?
        try {
            RegisterAllMethods();
        } catch (const std::exception& e) {
            MCP::Logger::Error("Failed to register methods: {}", e.what());
            _plugin_logprintf("[MCP] Failed to register methods: %s\n", e.what());
        }
        
        // 娉ㄥ�?x64dbg 鍥炶�?
        try {
            RegisterCallbacks();
        } catch (const std::exception& e) {
            MCP::Logger::Error("Failed to register callbacks: {}", e.what());
            _plugin_logprintf("[MCP] Failed to register callbacks: %s\n", e.what());
        }
        
        // 鍒濆鍖栦簨浠跺鐞嗗櫒
        try {
            MCP::EventCallbackHandler::Instance().Initialize();
        } catch (const std::exception& e) {
            MCP::Logger::Error("Failed to initialize event handler: {}", e.what());
            _plugin_logprintf("[MCP] Failed to initialize event handler: %s\n", e.what());
        }
        
        // Optional auto-start when plugin is loaded.
        try {
            if (MCP::ConfigManager::Instance().Get<bool>(kAutoStartConfigKey, false)) {
                _plugin_logputs("[MCP] Auto-start option enabled, starting MCP HTTP Server...");
                MCP::StartMcpHttpServer(false);
            }
        } catch (const std::exception& e) {
            MCP::Logger::Error("Failed to auto-start MCP HTTP server: {}", e.what());
            _plugin_logprintf("[MCP] Failed to auto-start MCP HTTP server: %s\n", e.what());
        }

        MCP::Logger::Info("Plugin initialized successfully");
        _plugin_logputs("[MCP] Plugin initialized successfully");
        return true;  // �?杩斿洖true琛ㄧず鍒濆鍖栨垚鍔?
    } catch (const std::exception& e) {
        // 鎹曡幏鎵€鏈夋湭澶勭悊鐨勫紓甯革紝闃叉宕╂�?
        _plugin_logprintf("[MCP] FATAL ERROR during initialization: %s\n", e.what());
        return false;  // �?杩斿洖false琛ㄧず鍒濆鍖栧け璐?
    } catch (...) {
        _plugin_logputs("[MCP] FATAL ERROR: Unknown exception during initialization");
        return false;  // �?杩斿洖false琛ㄧず鍒濆鍖栧け璐?
    }
}

/**
 * @brief 鎻掍欢鍋滄
 */
extern "C" __declspec(dllexport) void plugstop() {
    MCP::Logger::Info("Plugin stopping...");
    
    // 鍋滄�?MCP HTTP 鏈嶅姟鍣?
    try {
        if (g_mcpHttpServer && g_mcpHttpServer->IsRunning()) {
            g_mcpHttpServer->Stop();
        }
        g_mcpHttpServer.reset();
    } catch (...) {}
    
    // 娓呯悊浜嬩欢澶勭悊鍣?
    MCP::EventCallbackHandler::Instance().Cleanup();
    
    MCP::Logger::Info("Plugin stopped");
    MCP::Logger::Shutdown();
}

/**
 * @brief 鎻掍欢璁剧疆锛堝垱寤鸿彍鍗曪�?
 * @return true琛ㄧず璁剧疆鎴愬姛锛宖alse琛ㄧず澶辫触
 */
extern "C" __declspec(dllexport) bool plugsetup(PLUG_SETUPSTRUCT* setupStruct) {
    _plugin_logputs("[MCP] Setting up plugin menu...");
    
    // 娣诲姞鎻掍欢鑿滃�?
    g_menuHandle = _plugin_menuadd(setupStruct->hMenu, "&MCP Server");
    
    if (g_menuHandle) {
        // �?浣跨�?_plugin_menuaddentry 娣诲姞鍙偣鍑荤殑鑿滃崟�?
        _plugin_menuaddentry(g_menuHandle, MENU_START_MCP_HTTP, "Start &MCP HTTP Server");
        _plugin_menuaddentry(g_menuHandle, MENU_STOP_MCP_HTTP, "Stop M&CP HTTP Server");
        _plugin_menuaddentry(g_menuHandle, MENU_AUTO_START_MCP, "Auto Start MCP on Plugin &Load");
        _plugin_menuaddseparator(g_menuHandle);  // 鍒嗛殧绗?
        _plugin_menuaddentry(g_menuHandle, MENU_EDIT_CONFIG, "&Edit Config");
        _plugin_menuaddentry(g_menuHandle, MENU_SHOW_CONFIG, "Show &Config");
        _plugin_menuaddseparator(g_menuHandle);  // 鍒嗛殧绗?
        _plugin_menuaddentry(g_menuHandle, MENU_ABOUT, "&About");
        
        MCP::UpdateAutoStartMenuCheck();

        _plugin_logputs("[MCP] Plugin menu created successfully");
        _plugin_logprintf("[MCP] Menu handle: %d\n", g_menuHandle);
        _plugin_logprintf("[MCP] Menu entries added\n");
    } else {
        _plugin_logputs("[MCP] ERROR: Failed to create plugin menu!");
        return false;  // �?鑿滃崟鍒涘缓澶辫�?
    }
    
    // 浣跨敤x64dbg鐨勬棩蹇楃郴缁燂紝鍥犱负鑷畾涔塋ogger鍙兘杩樻湭鍒濆鍖?
    try {
        MCP::Logger::Debug("Plugin menu created");
    } catch (...) {
        // 濡傛灉Logger鏈垵濮嬪寲锛屽拷鐣ラ敊�?
    }
    
    return true;  // �?杩斿洖true琛ㄧず璁剧疆鎴愬�?
}

/**
 * @brief DLL 涓诲嚱鏁?
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hModule;  // 淇濆瓨妯″潡鍙ユ�?
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
