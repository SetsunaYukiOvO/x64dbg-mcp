#include "ConfigEditor.h"
#include "resource.h"
#include "../core/Logger.h"
#include "../core/ConfigManager.h"
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>

#pragma comment(lib, "comctl32.lib")

namespace MCP {

std::string ConfigEditor::s_configPath;
json ConfigEditor::s_config;

bool ConfigEditor::Show(HMODULE hModule, HWND parentWindow, const std::string& configPath) {
    s_configPath = configPath;
    
    // 添加调试日志
    Logger::Info("ConfigEditor::Show called with configPath: {}", configPath);
    
    // 加载配置
    try {
        std::ifstream file(configPath);
        if (file.is_open()) {
            file >> s_config;
            file.close();
            if (!s_config.is_object()) {
                Logger::Warning("Config root is not a JSON object, fallback to default config");
                s_config = ConfigManager::Instance().GetDefaultConfig();
            }
            Logger::Info("Config file loaded successfully");
        } else {
            // 如果配置文件不存在,使用默认配置
            Logger::Warning("Config file not found, using default configuration: {}", configPath);
            s_config = ConfigManager::Instance().GetDefaultConfig();
            
            // 尝试创建配置文件目录
            std::filesystem::path path(configPath);
            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path());
            }
        }
    } catch (const std::exception& e) {
        Logger::Error("Failed to parse config: {}", e.what());
        MessageBoxA(parentWindow, "Failed to parse config file!", "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    Logger::Info("Creating dialog with module handle: 0x{:X}", reinterpret_cast<uintptr_t>(hModule));
    
    // 创建对话框
    INT_PTR result = DialogBoxParamA(
        hModule,  // 使用插件的模块句柄
        MAKEINTRESOURCEA(IDD_CONFIG_EDITOR),
        parentWindow,
        DialogProc,
        0
    );
    
    if (result == -1) {
        DWORD error = GetLastError();
        Logger::Error("DialogBoxParamA failed with error: {}", error);
        char errorMsg[256];
        sprintf_s(errorMsg, "Failed to create dialog! Error code: %lu", error);
        MessageBoxA(parentWindow, errorMsg, "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    Logger::Info("Dialog closed with result: {}", result);
    return result == IDOK;
}

INT_PTR CALLBACK ConfigEditor::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (uMsg) {
        case WM_INITDIALOG: {
            // 设置对话框标题
            SetWindowTextA(hwndDlg, "MCP Server Configuration Editor");
            
            // 初始化控件
            LoadConfigToControls(hwndDlg, s_config);
            
            // 居中显示
            RECT rc;
            GetWindowRect(hwndDlg, &rc);
            int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
            SetWindowPos(hwndDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK_SAVE: {
                    if (SaveConfig(hwndDlg, s_configPath)) {
                        MessageBoxA(hwndDlg, "Configuration saved successfully!", "Success", MB_OK | MB_ICONINFORMATION);
                        EndDialog(hwndDlg, IDOK);
                    } else {
                        MessageBoxA(hwndDlg, "Failed to save configuration!", "Error", MB_OK | MB_ICONERROR);
                    }
                    return TRUE;
                }
                
                case IDCANCEL_CLOSE: {
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }
                
                case IDC_METHOD_ADD: {
                    char methodName[256] = {0};
                    GetDlgItemTextA(hwndDlg, IDC_METHOD_INPUT, methodName, sizeof(methodName));
                    
                    if (strlen(methodName) > 0) {
                        // 添加到列表框
                        HWND listBox = GetDlgItem(hwndDlg, IDC_METHODS_LIST);
                        SendMessageA(listBox, LB_ADDSTRING, 0, (LPARAM)methodName);
                        
                        // 清空输入框
                        SetDlgItemTextA(hwndDlg, IDC_METHOD_INPUT, "");
                    }
                    return TRUE;
                }
                
                case IDC_METHOD_REMOVE: {
                    HWND listBox = GetDlgItem(hwndDlg, IDC_METHODS_LIST);
                    LRESULT sel = SendMessageA(listBox, LB_GETCURSEL, 0, 0);
                    if (sel != LB_ERR) {
                        SendMessageA(listBox, LB_DELETESTRING, static_cast<WPARAM>(sel), 0);
                    }
                    return TRUE;
                }
            }
            break;
        }
        
        case WM_CLOSE: {
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
    }
    
    return FALSE;
}

void ConfigEditor::LoadConfigToControls(HWND hwndDlg, const json& config) {
    // Server settings
    SetDlgItemTextA(hwndDlg, IDC_SERVER_ADDRESS, 
        config.value("server", json::object()).value("address", "127.0.0.1").c_str());
    SetDlgItemInt(hwndDlg, IDC_SERVER_PORT, 
        config.value("server", json::object()).value("port", 3000), FALSE);
    
    // Permissions
    auto perms = config.value("permissions", json::object());
    CheckDlgButton(hwndDlg, IDC_ALLOW_MEMORY_WRITE, 
        perms.value("allow_memory_write", true) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_ALLOW_REGISTER_WRITE, 
        perms.value("allow_register_write", true) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_ALLOW_SCRIPT_EXEC, 
        perms.value("allow_script_execution", true) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_ALLOW_BREAKPOINT_MOD, 
        perms.value("allow_breakpoint_modification", true) ? BST_CHECKED : BST_UNCHECKED);
    
    // Allowed methods
    HWND listBox = GetDlgItem(hwndDlg, IDC_METHODS_LIST);
    SendMessageA(listBox, LB_RESETCONTENT, 0, 0);
    
    auto methods = perms.value("allowed_methods", json::array());
    for (const auto& method : methods) {
        if (method.is_string()) {
            SendMessageA(listBox, LB_ADDSTRING, 0, (LPARAM)method.get<std::string>().c_str());
        }
    }
    
    // Logging
    auto logging = config.value("logging", json::object());
    CheckDlgButton(hwndDlg, IDC_LOG_ENABLED, 
        logging.value("enabled", true) ? BST_CHECKED : BST_UNCHECKED);
    
    std::string logLevel = logging.value("level", "info");
    HWND comboBox = GetDlgItem(hwndDlg, IDC_LOG_LEVEL);
    SendMessageA(comboBox, CB_RESETCONTENT, 0, 0);
    SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)"debug");
    SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)"info");
    SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)"warning");
    SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)"error");
    
    if (logLevel == "debug") SendMessageA(comboBox, CB_SETCURSEL, 0, 0);
    else if (logLevel == "info") SendMessageA(comboBox, CB_SETCURSEL, 1, 0);
    else if (logLevel == "warning") SendMessageA(comboBox, CB_SETCURSEL, 2, 0);
    else if (logLevel == "error") SendMessageA(comboBox, CB_SETCURSEL, 3, 0);
    
    SetDlgItemTextA(hwndDlg, IDC_LOG_FILE, 
        logging.value("file", "x64dbg_mcp.log").c_str());
    SetDlgItemInt(hwndDlg, IDC_LOG_MAX_SIZE, 
        logging.value("max_file_size_mb", 10), FALSE);
    CheckDlgButton(hwndDlg, IDC_LOG_CONSOLE, 
        logging.value("console_output", true) ? BST_CHECKED : BST_UNCHECKED);
    
    // Timeouts
    auto timeout = config.value("timeout", json::object());
    SetDlgItemInt(hwndDlg, IDC_TIMEOUT_REQUEST, 
        timeout.value("request_timeout_ms", 30000), FALSE);
    SetDlgItemInt(hwndDlg, IDC_TIMEOUT_STEP, 
        timeout.value("step_timeout_ms", 10000), FALSE);
    SetDlgItemInt(hwndDlg, IDC_TIMEOUT_MEMORY, 
        timeout.value("memory_read_timeout_ms", 5000), FALSE);
    
    // Features
    auto features = config.value("features", json::object());
    CheckDlgButton(hwndDlg, IDC_FEATURE_NOTIFICATIONS, 
        features.value("enable_notifications", true) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_FEATURE_HEARTBEAT, 
        features.value("enable_heartbeat", true) ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwndDlg, IDC_HEARTBEAT_INTERVAL, 
        features.value("heartbeat_interval_seconds", 30), FALSE);
    CheckDlgButton(hwndDlg, IDC_FEATURE_BATCH, 
        features.value("enable_batch_requests", true) ? BST_CHECKED : BST_UNCHECKED);
}

json ConfigEditor::GetConfigFromControls(HWND hwndDlg) {
    json config;
    
    // Server settings
    char buffer[256];
    GetDlgItemTextA(hwndDlg, IDC_SERVER_ADDRESS, buffer, sizeof(buffer));
    config["server"]["address"] = buffer;
    config["server"]["port"] = GetDlgItemInt(hwndDlg, IDC_SERVER_PORT, NULL, FALSE);
    
    // Permissions
    config["permissions"]["allow_memory_write"] = 
        IsDlgButtonChecked(hwndDlg, IDC_ALLOW_MEMORY_WRITE) == BST_CHECKED;
    config["permissions"]["allow_register_write"] = 
        IsDlgButtonChecked(hwndDlg, IDC_ALLOW_REGISTER_WRITE) == BST_CHECKED;
    config["permissions"]["allow_script_execution"] = 
        IsDlgButtonChecked(hwndDlg, IDC_ALLOW_SCRIPT_EXEC) == BST_CHECKED;
    config["permissions"]["allow_breakpoint_modification"] = 
        IsDlgButtonChecked(hwndDlg, IDC_ALLOW_BREAKPOINT_MOD) == BST_CHECKED;
    
    // Allowed methods
    json methodsArray = json::array();
    HWND listBox = GetDlgItem(hwndDlg, IDC_METHODS_LIST);
    const LRESULT countResult = SendMessageA(listBox, LB_GETCOUNT, 0, 0);
    const int count = (countResult < 0) ? 0 : static_cast<int>(countResult);
    
    for (int i = 0; i < count; i++) {
        char methodName[256];
        SendMessageA(listBox, LB_GETTEXT, i, (LPARAM)methodName);
        methodsArray.push_back(methodName);
    }
    config["permissions"]["allowed_methods"] = methodsArray;
    
    // Logging
    config["logging"]["enabled"] = 
        IsDlgButtonChecked(hwndDlg, IDC_LOG_ENABLED) == BST_CHECKED;
    
    HWND comboBox = GetDlgItem(hwndDlg, IDC_LOG_LEVEL);
    const LRESULT selectedIndex = SendMessageA(comboBox, CB_GETCURSEL, 0, 0);
    const int sel = (selectedIndex == CB_ERR) ? -1 : static_cast<int>(selectedIndex);
    const char* levels[] = {"debug", "info", "warning", "error"};
    config["logging"]["level"] = levels[sel >= 0 && sel < 4 ? sel : 1];
    
    GetDlgItemTextA(hwndDlg, IDC_LOG_FILE, buffer, sizeof(buffer));
    config["logging"]["file"] = buffer;
    config["logging"]["max_file_size_mb"] = 
        GetDlgItemInt(hwndDlg, IDC_LOG_MAX_SIZE, NULL, FALSE);
    config["logging"]["console_output"] = 
        IsDlgButtonChecked(hwndDlg, IDC_LOG_CONSOLE) == BST_CHECKED;
    
    // Timeouts
    config["timeout"]["request_timeout_ms"] = 
        GetDlgItemInt(hwndDlg, IDC_TIMEOUT_REQUEST, NULL, FALSE);
    config["timeout"]["step_timeout_ms"] = 
        GetDlgItemInt(hwndDlg, IDC_TIMEOUT_STEP, NULL, FALSE);
    config["timeout"]["memory_read_timeout_ms"] = 
        GetDlgItemInt(hwndDlg, IDC_TIMEOUT_MEMORY, NULL, FALSE);
    
    // Features
    config["features"]["enable_notifications"] = 
        IsDlgButtonChecked(hwndDlg, IDC_FEATURE_NOTIFICATIONS) == BST_CHECKED;
    config["features"]["enable_heartbeat"] = 
        IsDlgButtonChecked(hwndDlg, IDC_FEATURE_HEARTBEAT) == BST_CHECKED;
    config["features"]["heartbeat_interval_seconds"] = 
        GetDlgItemInt(hwndDlg, IDC_HEARTBEAT_INTERVAL, NULL, FALSE);
    config["features"]["enable_batch_requests"] = 
        IsDlgButtonChecked(hwndDlg, IDC_FEATURE_BATCH) == BST_CHECKED;
    // Keep menu-managed toggle persisted even though it has no dialog control.
    config["features"]["auto_start_mcp_on_plugin_load"] =
        s_config.value("features", json::object()).value("auto_start_mcp_on_plugin_load", false);
    
    // 保留version字段
    config["version"] = s_config.value("version", "1.0.5");
    
    return config;
}

bool ConfigEditor::SaveConfig(HWND hwndDlg, const std::string& configPath) {
    try {
        json newConfig = GetConfigFromControls(hwndDlg);
        
        // 保存到文件
        std::ofstream file(configPath);
        if (!file.is_open()) {
            Logger::Error("Failed to open config file for writing: {}", configPath);
            return false;
        }
        
        file << newConfig.dump(2);  // 格式化输出,缩进2个空格
        file.close();
        
        // 更新全局配置
        s_config = newConfig;
        
        Logger::Info("Configuration saved successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Failed to save config: {}", e.what());
        return false;
    }
}

} // namespace MCP
