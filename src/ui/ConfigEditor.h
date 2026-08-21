#pragma once

#include <windows.h>
#include <string>
#include <nlohmann/json.hpp>

namespace MCP {

using json = nlohmann::json;

class ConfigEditor {
public:
    static bool Show(HMODULE hModule, HWND parentWindow, const std::string& configPath);

private:
    static INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void InitializeControls(HWND hwndDlg, const json& config);
    static bool SaveConfig(HWND hwndDlg, const std::string& configPath);
    static void LoadConfigToControls(HWND hwndDlg, const json& config);
    static json GetConfigFromControls(HWND hwndDlg);
    static void ShowPage(HWND hwndDlg, int page);
    static void ResizeLayout(HWND hwndDlg);
    
    // 控件ID定义
    enum ControlIDs {
        IDC_SERVER_ADDRESS = 1001,
        IDC_SERVER_PORT = 1002,
        IDC_ALLOW_MEMORY_WRITE = 1003,
        IDC_ALLOW_REGISTER_WRITE = 1004,
        IDC_ALLOW_SCRIPT_EXEC = 1005,
        IDC_ALLOW_BREAKPOINT_MOD = 1006,
        IDC_LOG_ENABLED = 1007,
        IDC_LOG_LEVEL = 1008,
        IDC_LOG_FILE = 1009,
        IDC_LOG_MAX_SIZE = 1010,
        IDC_LOG_CONSOLE = 1011,
        IDC_TIMEOUT_REQUEST = 1012,
        IDC_TIMEOUT_STEP = 1013,
        IDC_TIMEOUT_MEMORY = 1014,
        IDC_FEATURE_NOTIFICATIONS = 1015,
        IDC_FEATURE_HEARTBEAT = 1016,
        IDC_HEARTBEAT_INTERVAL = 1017,
        IDC_FEATURE_BATCH = 1018,
        IDC_METHODS_LIST = 1019,
        IDC_METHOD_ADD = 1020,
        IDC_METHOD_REMOVE = 1021,
        IDC_METHOD_INPUT = 1022,
        IDOK_SAVE = 1,
        IDCANCEL_CLOSE = 2
    };
    
    static std::string s_configPath;
    static json s_config;
    static int s_currentPage;
    static int s_initialClientWidth;
    static int s_initialClientHeight;
};

} // namespace MCP
