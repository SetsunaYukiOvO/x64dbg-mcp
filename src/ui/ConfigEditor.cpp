#include "ConfigEditor.h"
#include "resource.h"
#include "../core/Logger.h"
#include "../core/ConfigManager.h"
#include <commctrl.h>
#include <fstream>
#include <filesystem>
#include <array>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "comctl32.lib")

namespace MCP {

std::string ConfigEditor::s_configPath;
json ConfigEditor::s_config;
int ConfigEditor::s_currentPage = 0;
int ConfigEditor::s_initialClientWidth = 0;
int ConfigEditor::s_initialClientHeight = 0;

namespace {

std::unordered_map<int, RECT> g_baseRects;

constexpr std::array<std::array<int, 6>, 5> kPages = {{
    {{IDC_SERVER_GROUP, IDC_LABEL_SERVER_ADDRESS, IDC_SERVER_ADDRESS, IDC_LABEL_SERVER_PORT, IDC_SERVER_PORT, IDC_FEATURE_AUTOSTART}},
    {{IDC_SECURITY_GROUP, IDC_LABEL_SECURITY_ORIGINS, IDC_SECURITY_ORIGINS, IDC_LABEL_SECURITY_HOSTS, IDC_SECURITY_HOSTS, IDC_AUTH_ENABLED}},
    {{IDC_PERMISSIONS_GROUP, IDC_ALLOW_MEMORY_WRITE, IDC_ALLOW_REGISTER_WRITE, IDC_ALLOW_SCRIPT_EXEC, IDC_ALLOW_BREAKPOINT_MOD, IDC_METHODS_GROUP}},
    {{IDC_TIMEOUT_GROUP, IDC_LABEL_TIMEOUT_REQUEST, IDC_TIMEOUT_REQUEST, IDC_LABEL_TIMEOUT_STEP, IDC_TIMEOUT_STEP, IDC_LABEL_TIMEOUT_MEMORY}},
    {{IDC_LOGGING_GROUP, IDC_LOG_ENABLED, IDC_LABEL_LOG_LEVEL, IDC_LOG_LEVEL, IDC_LABEL_LOG_FILE, IDC_LOG_FILE}}
}};

constexpr std::array<int, 10> kSecurityExtra = {
    IDC_LABEL_AUTH_TOKEN, IDC_AUTH_TOKEN, IDC_LABEL_SECURITY_HINT,
    IDC_LABEL_SECURITY_ORIGINS, IDC_LABEL_SECURITY_HOSTS, IDC_SECURITY_ORIGINS,
    IDC_SECURITY_HOSTS, IDC_AUTH_ENABLED, IDC_LABEL_SECURITY_ALLOWLISTS,
    IDC_LABEL_SECURITY_AUTH
};
constexpr std::array<int, 9> kPermissionExtra = {
    IDC_METHODS_LIST, IDC_LABEL_METHOD_PATTERN, IDC_METHOD_INPUT,
    IDC_METHOD_ADD, IDC_METHOD_REMOVE, IDC_METHODS_GROUP, IDC_PERMISSIONS_GROUP,
    IDC_ALLOW_MEMORY_WRITE, IDC_ALLOW_BREAKPOINT_MOD
};
constexpr std::array<int, 11> kRuntimeExtra = {
    IDC_LABEL_TIMEOUT_MEMORY, IDC_TIMEOUT_MEMORY, IDC_FEATURES_GROUP,
    IDC_FEATURE_NOTIFICATIONS, IDC_FEATURE_HEARTBEAT, IDC_LABEL_HEARTBEAT_INTERVAL,
    IDC_HEARTBEAT_INTERVAL, IDC_FEATURE_BATCH, IDC_TIMEOUT_GROUP,
    IDC_LABEL_RUNTIME_TIMEOUTS, IDC_LABEL_RUNTIME_FEATURES
};
constexpr std::array<int, 5> kLoggingExtra = {
    IDC_LABEL_LOG_MAX_SIZE, IDC_LOG_MAX_SIZE, IDC_LOG_CONSOLE, IDC_LOGGING_GROUP, IDC_LOG_FILE
};

void SetVisible(HWND dialog, int id, bool visible) {
    if (HWND control = GetDlgItem(dialog, id)) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

template <size_t N>
void SetVisible(HWND dialog, const std::array<int, N>& controls, bool visible) {
    for (int id : controls) SetVisible(dialog, id, visible);
}

std::string GetText(HWND dialog, int id) {
    const int length = GetWindowTextLengthA(GetDlgItem(dialog, id));
    std::vector<char> buffer(static_cast<size_t>(length) + 1, '\0');
    GetDlgItemTextA(dialog, id, buffer.data(), length + 1);
    return buffer.data();
}

void SetText(HWND dialog, int id, const std::string& value) {
    SetDlgItemTextA(dialog, id, value.c_str());
}

void Trim(std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
}

std::string JsonArrayToLines(const json& values) {
    std::string result;
    if (!values.is_array()) return result;
    for (const auto& value : values) {
        if (!value.is_string()) continue;
        if (!result.empty()) result += "\r\n";
        result += value.get<std::string>();
    }
    return result;
}

json LinesToJsonArray(const std::string& lines) {
    json values = json::array();
    size_t start = 0;
    while (start <= lines.size()) {
        const size_t end = lines.find('\n', start);
        std::string line = lines.substr(start, end == std::string::npos ? std::string::npos : end - start);
        Trim(line);
        if (!line.empty()) values.push_back(line);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return values;
}

bool IsNonLoopbackAddress(const std::string& address) {
    return address != "127.0.0.1" && address != "localhost" && address != "::1";
}

void RecordBaseRect(HWND dialog, int id) {
    HWND control = GetDlgItem(dialog, id);
    if (!control) return;
    RECT rect{};
    GetWindowRect(control, &rect);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&rect), 2);
    g_baseRects[id] = rect;
}

void Place(HWND dialog, int id, int x, int y, int width, int height) {
    if (HWND control = GetDlgItem(dialog, id)) {
        MoveWindow(control, x, y, width, height, TRUE);
    }
}

} // namespace

bool ConfigEditor::Show(HMODULE hModule, HWND parentWindow, const std::string& configPath) {
    s_configPath = configPath;
    try {
        std::ifstream file(configPath);
        if (file.is_open()) {
            file >> s_config;
            if (!s_config.is_object()) s_config = ConfigManager::Instance().GetDefaultConfig();
        } else {
            s_config = ConfigManager::Instance().GetDefaultConfig();
            std::filesystem::path path(configPath);
            if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        }
    } catch (const std::exception& e) {
        Logger::Error("Failed to parse config: {}", e.what());
        MessageBoxA(parentWindow, "Failed to parse config file.", "MCP Configuration", MB_OK | MB_ICONERROR);
        return false;
    }

    const INT_PTR result = DialogBoxParamA(hModule, MAKEINTRESOURCEA(IDD_CONFIG_EDITOR), parentWindow, DialogProc, 0);
    if (result == -1) {
        Logger::Error("DialogBoxParamA failed: {}", GetLastError());
        return false;
    }
    return result == IDOK;
}

INT_PTR CALLBACK ConfigEditor::DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowTextA(hwndDlg, "MCP Server Configuration");
        // Resource dialog units vary with the host DPI. Use a compact physical
        // size and lay out all controls from the actual client rectangle below.
        SetWindowPos(hwndDlg, nullptr, 0, 0, 720, 500, SWP_NOMOVE | SWP_NOZORDER);
        HWND tabs = GetDlgItem(hwndDlg, IDC_TAB_SETTINGS);
        const char* labels[] = {"Server", "Security", "Permissions", "Runtime", "Logging"};
        for (int i = 0; i < 5; ++i) {
            TCITEMA item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<char*>(labels[i]);
            TabCtrl_InsertItem(tabs, i, &item);
        }
        LoadConfigToControls(hwndDlg, s_config);
        SendMessageA(GetDlgItem(hwndDlg, IDC_AUTH_TOKEN), EM_LIMITTEXT, 512, 0);
        RECT client{};
        GetClientRect(hwndDlg, &client);
        s_initialClientWidth = client.right;
        s_initialClientHeight = client.bottom;
        g_baseRects.clear();
        s_currentPage = 0;
        ShowPage(hwndDlg, 0);
        return TRUE;
    }
    case WM_NOTIFY:
        if (reinterpret_cast<LPNMHDR>(lParam)->idFrom == IDC_TAB_SETTINGS &&
            reinterpret_cast<LPNMHDR>(lParam)->code == TCN_SELCHANGE) {
            ShowPage(hwndDlg, TabCtrl_GetCurSel(GetDlgItem(hwndDlg, IDC_TAB_SETTINGS)));
            return TRUE;
        }
        break;
    case WM_SIZE:
        ResizeLayout(hwndDlg);
        return TRUE;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        RECT minRect{0, 0, 600, 420};
        AdjustWindowRectEx(&minRect, GetWindowLongA(hwndDlg, GWL_STYLE), FALSE, GetWindowLongA(hwndDlg, GWL_EXSTYLE));
        info->ptMinTrackSize.x = minRect.right - minRect.left;
        info->ptMinTrackSize.y = minRect.bottom - minRect.top;
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (SaveConfig(hwndDlg, s_configPath)) EndDialog(hwndDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        case IDC_METHOD_ADD: {
            const std::string method = GetText(hwndDlg, IDC_METHOD_INPUT);
            if (!method.empty()) {
                SendMessageA(GetDlgItem(hwndDlg, IDC_METHODS_LIST), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(method.c_str()));
                SetText(hwndDlg, IDC_METHOD_INPUT, "");
            }
            return TRUE;
        }
        case IDC_METHOD_REMOVE: {
            HWND list = GetDlgItem(hwndDlg, IDC_METHODS_LIST);
            const LRESULT selected = SendMessageA(list, LB_GETCURSEL, 0, 0);
            if (selected != LB_ERR) SendMessageA(list, LB_DELETESTRING, static_cast<WPARAM>(selected), 0);
            return TRUE;
        }
        }
        break;
    case WM_CLOSE:
        EndDialog(hwndDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void ConfigEditor::ShowPage(HWND hwndDlg, int page) {
    const std::array<int, 6> server = kPages[0];
    const std::array<int, 6> security = kPages[1];
    const std::array<int, 6> permissions = kPages[2];
    const std::array<int, 6> runtime = kPages[3];
    const std::array<int, 6> logging = kPages[4];
    SetVisible(hwndDlg, server, page == 0);
    SetVisible(hwndDlg, security, page == 1);
    SetVisible(hwndDlg, permissions, page == 2);
    SetVisible(hwndDlg, runtime, page == 3);
    SetVisible(hwndDlg, logging, page == 4);
    SetVisible(hwndDlg, kSecurityExtra, page == 1);
    SetVisible(hwndDlg, kPermissionExtra, page == 2);
    SetVisible(hwndDlg, kRuntimeExtra, page == 3);
    SetVisible(hwndDlg, kLoggingExtra, page == 4);
    s_currentPage = page;
    ResizeLayout(hwndDlg);
}

void ConfigEditor::ResizeLayout(HWND hwndDlg) {
    RECT client{};
    GetClientRect(hwndDlg, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int left = 28;
    const int right = 28;
    const int bodyWidth = std::max(360, width - left - right);
    const int footerY = height - 42;
    const int fieldX = left + 120;
    const int fieldWidth = std::max(160, bodyWidth - 140);

    Place(hwndDlg, IDC_TAB_SETTINGS, 10, 8, width - 20, height - 62);
    Place(hwndDlg, IDC_FOOTER_STATUS, 18, footerY + 10, std::max(250, width - 210), 18);
    Place(hwndDlg, IDOK, width - 146, footerY + 4, 62, 24);
    Place(hwndDlg, IDCANCEL, width - 76, footerY + 4, 62, 24);

    if (s_currentPage == 0) {
        Place(hwndDlg, IDC_SERVER_GROUP, left, 46, bodyWidth, 126);
        Place(hwndDlg, IDC_LABEL_SERVER_ADDRESS, left + 16, 76, 92, 18);
        Place(hwndDlg, IDC_SERVER_ADDRESS, fieldX, 72, fieldWidth, 24);
        Place(hwndDlg, IDC_LABEL_SERVER_PORT, left + 16, 108, 92, 18);
        Place(hwndDlg, IDC_SERVER_PORT, fieldX, 104, 100, 24);
        Place(hwndDlg, IDC_FEATURE_AUTOSTART, left + 16, 142, bodyWidth - 32, 22);
    } else if (s_currentPage == 1) {
        const int listHeight = std::max(34, (footerY - 312) / 2);
        Place(hwndDlg, IDC_SECURITY_GROUP, left, 46, bodyWidth, footerY - 54);
        Place(hwndDlg, IDC_LABEL_SECURITY_ALLOWLISTS, left + 16, 72, 220, 18);
        Place(hwndDlg, IDC_LABEL_SECURITY_ORIGINS, left + 16, 98, 220, 18);
        Place(hwndDlg, IDC_SECURITY_ORIGINS, left + 16, 118, bodyWidth - 32, listHeight);
        const int hostLabelY = 126 + listHeight;
        Place(hwndDlg, IDC_LABEL_SECURITY_HOSTS, left + 16, hostLabelY, 220, 18);
        Place(hwndDlg, IDC_SECURITY_HOSTS, left + 16, hostLabelY + 20, bodyWidth - 32, listHeight);
        const int authY = hostLabelY + listHeight + 30;
        Place(hwndDlg, IDC_LABEL_SECURITY_AUTH, left + 16, authY, 170, 18);
        Place(hwndDlg, IDC_AUTH_ENABLED, left + 16, authY + 24, 130, 22);
        Place(hwndDlg, IDC_LABEL_AUTH_TOKEN, left + 156, authY + 26, 42, 18);
        Place(hwndDlg, IDC_AUTH_TOKEN, left + 204, authY + 22, bodyWidth - 220, 24);
        Place(hwndDlg, IDC_LABEL_SECURITY_HINT, left + 16, authY + 56, bodyWidth - 32, 18);
    } else if (s_currentPage == 2) {
        const int permissionHeight = 132;
        const int methodsY = 46 + permissionHeight + 12;
        Place(hwndDlg, IDC_PERMISSIONS_GROUP, left, 46, bodyWidth, permissionHeight);
        Place(hwndDlg, IDC_ALLOW_MEMORY_WRITE, left + 16, 74, 220, 20);
        Place(hwndDlg, IDC_ALLOW_REGISTER_WRITE, left + 16, 98, 220, 20);
        Place(hwndDlg, IDC_ALLOW_SCRIPT_EXEC, left + 16, 122, 220, 20);
        Place(hwndDlg, IDC_ALLOW_BREAKPOINT_MOD, left + 16, 146, 240, 20);
        Place(hwndDlg, IDC_METHODS_GROUP, left, methodsY, bodyWidth, footerY - methodsY - 8);
        Place(hwndDlg, IDC_METHODS_LIST, left + 16, methodsY + 28, bodyWidth - 32, std::max(48, footerY - methodsY - 92));
        Place(hwndDlg, IDC_LABEL_METHOD_PATTERN, left + 16, footerY - 54, 82, 18);
        Place(hwndDlg, IDC_METHOD_INPUT, left + 102, footerY - 58, bodyWidth - 250, 24);
        Place(hwndDlg, IDC_METHOD_ADD, left + bodyWidth - 136, footerY - 58, 58, 24);
        Place(hwndDlg, IDC_METHOD_REMOVE, left + bodyWidth - 70, footerY - 58, 58, 24);
    } else if (s_currentPage == 3) {
        Place(hwndDlg, IDC_TIMEOUT_GROUP, left, 46, bodyWidth, 224);
        const int rightColumn = left + bodyWidth / 2 + 10;
        Place(hwndDlg, IDC_LABEL_RUNTIME_TIMEOUTS, left + 16, 74, 170, 18);
        Place(hwndDlg, IDC_LABEL_TIMEOUT_REQUEST, left + 16, 106, 78, 18);
        Place(hwndDlg, IDC_TIMEOUT_REQUEST, left + 100, 102, 112, 24);
        Place(hwndDlg, IDC_LABEL_TIMEOUT_STEP, left + 16, 138, 78, 18);
        Place(hwndDlg, IDC_TIMEOUT_STEP, left + 100, 134, 112, 24);
        Place(hwndDlg, IDC_LABEL_TIMEOUT_MEMORY, left + 16, 170, 78, 18);
        Place(hwndDlg, IDC_TIMEOUT_MEMORY, left + 100, 166, 112, 24);
        Place(hwndDlg, IDC_LABEL_RUNTIME_FEATURES, rightColumn, 74, 120, 18);
        Place(hwndDlg, IDC_FEATURE_NOTIFICATIONS, rightColumn, 106, 180, 20);
        Place(hwndDlg, IDC_FEATURE_HEARTBEAT, rightColumn, 138, 150, 20);
        Place(hwndDlg, IDC_LABEL_HEARTBEAT_INTERVAL, rightColumn, 170, 76, 18);
        Place(hwndDlg, IDC_HEARTBEAT_INTERVAL, rightColumn + 82, 166, 82, 24);
        Place(hwndDlg, IDC_FEATURE_BATCH, rightColumn, 202, 180, 20);
    } else if (s_currentPage == 4) {
        Place(hwndDlg, IDC_LOGGING_GROUP, left, 46, bodyWidth, 176);
        const int rightColumn = left + bodyWidth / 2 + 10;
        Place(hwndDlg, IDC_LOG_ENABLED, left + 16, 76, 150, 20);
        Place(hwndDlg, IDC_LOG_CONSOLE, rightColumn, 76, 160, 20);
        Place(hwndDlg, IDC_LABEL_LOG_LEVEL, left + 16, 110, 62, 18);
        Place(hwndDlg, IDC_LOG_LEVEL, left + 84, 106, 132, 160);
        Place(hwndDlg, IDC_LABEL_LOG_MAX_SIZE, rightColumn, 110, 88, 18);
        Place(hwndDlg, IDC_LOG_MAX_SIZE, rightColumn + 96, 106, 76, 24);
        Place(hwndDlg, IDC_LABEL_LOG_FILE, left + 16, 146, 62, 18);
        Place(hwndDlg, IDC_LOG_FILE, left + 84, 142, bodyWidth - 100, 24);
    }
}

void ConfigEditor::LoadConfigToControls(HWND hwndDlg, const json& config) {
    const auto server = config.value("server", json::object());
    SetText(hwndDlg, IDC_SERVER_ADDRESS, server.value("address", "127.0.0.1"));
    SetDlgItemInt(hwndDlg, IDC_SERVER_PORT, server.value("port", 3000), FALSE);
    const auto permissions = config.value("permissions", json::object());
    CheckDlgButton(hwndDlg, IDC_ALLOW_MEMORY_WRITE, permissions.value("allow_memory_write", false) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_ALLOW_REGISTER_WRITE, permissions.value("allow_register_write", false) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_ALLOW_SCRIPT_EXEC, permissions.value("allow_script_execution", false) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_ALLOW_BREAKPOINT_MOD, permissions.value("allow_breakpoint_modification", true) ? BST_CHECKED : BST_UNCHECKED);
    HWND methods = GetDlgItem(hwndDlg, IDC_METHODS_LIST);
    SendMessageA(methods, LB_RESETCONTENT, 0, 0);
    for (const auto& method : permissions.value("allowed_methods", json::array())) {
        if (method.is_string()) SendMessageA(methods, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(method.get<std::string>().c_str()));
    }
    const auto security = config.value("security", json::object());
    SetText(hwndDlg, IDC_SECURITY_ORIGINS, JsonArrayToLines(security.value("origin_allowlist", json::array())));
    SetText(hwndDlg, IDC_SECURITY_HOSTS, JsonArrayToLines(security.value("host_allowlist", json::array())));
    CheckDlgButton(hwndDlg, IDC_AUTH_ENABLED, security.value("auth_enabled", false) ? BST_CHECKED : BST_UNCHECKED);
    SetText(hwndDlg, IDC_AUTH_TOKEN, security.value("auth_token", ""));
    const auto logging = config.value("logging", json::object());
    CheckDlgButton(hwndDlg, IDC_LOG_ENABLED, logging.value("enabled", true) ? BST_CHECKED : BST_UNCHECKED);
    HWND level = GetDlgItem(hwndDlg, IDC_LOG_LEVEL);
    for (const char* value : {"debug", "info", "warning", "error"}) SendMessageA(level, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    const std::string configuredLevel = logging.value("level", "info");
    SendMessageA(level, CB_SETCURSEL, configuredLevel == "debug" ? 0 : configuredLevel == "warning" ? 2 : configuredLevel == "error" ? 3 : 1, 0);
    SetText(hwndDlg, IDC_LOG_FILE, logging.value("file", "x64dbg_mcp.log"));
    SetDlgItemInt(hwndDlg, IDC_LOG_MAX_SIZE, logging.value("max_file_size_mb", 10), FALSE);
    CheckDlgButton(hwndDlg, IDC_LOG_CONSOLE, logging.value("console_output", true) ? BST_CHECKED : BST_UNCHECKED);
    const auto timeout = config.value("timeout", json::object());
    SetDlgItemInt(hwndDlg, IDC_TIMEOUT_REQUEST, timeout.value("request_timeout_ms", 30000), FALSE);
    SetDlgItemInt(hwndDlg, IDC_TIMEOUT_STEP, timeout.value("step_timeout_ms", 10000), FALSE);
    SetDlgItemInt(hwndDlg, IDC_TIMEOUT_MEMORY, timeout.value("memory_read_timeout_ms", 5000), FALSE);
    const auto features = config.value("features", json::object());
    CheckDlgButton(hwndDlg, IDC_FEATURE_NOTIFICATIONS, features.value("enable_notifications", true) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_FEATURE_HEARTBEAT, features.value("enable_heartbeat", true) ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwndDlg, IDC_HEARTBEAT_INTERVAL, features.value("heartbeat_interval_seconds", 30), FALSE);
    CheckDlgButton(hwndDlg, IDC_FEATURE_BATCH, features.value("enable_batch_requests", true) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_FEATURE_AUTOSTART, features.value("auto_start_mcp_on_plugin_load", false) ? BST_CHECKED : BST_UNCHECKED);
}

json ConfigEditor::GetConfigFromControls(HWND hwndDlg) {
    json config = s_config.is_object() ? s_config : ConfigManager::Instance().GetDefaultConfig();
    config["server"]["address"] = GetText(hwndDlg, IDC_SERVER_ADDRESS);
    config["server"]["port"] = GetDlgItemInt(hwndDlg, IDC_SERVER_PORT, nullptr, FALSE);
    config["permissions"]["allow_memory_write"] = IsDlgButtonChecked(hwndDlg, IDC_ALLOW_MEMORY_WRITE) == BST_CHECKED;
    config["permissions"]["allow_register_write"] = IsDlgButtonChecked(hwndDlg, IDC_ALLOW_REGISTER_WRITE) == BST_CHECKED;
    config["permissions"]["allow_script_execution"] = IsDlgButtonChecked(hwndDlg, IDC_ALLOW_SCRIPT_EXEC) == BST_CHECKED;
    config["permissions"]["allow_breakpoint_modification"] = IsDlgButtonChecked(hwndDlg, IDC_ALLOW_BREAKPOINT_MOD) == BST_CHECKED;
    json methods = json::array();
    HWND list = GetDlgItem(hwndDlg, IDC_METHODS_LIST);
    const int count = static_cast<int>(SendMessageA(list, LB_GETCOUNT, 0, 0));
    for (int index = 0; index < count; ++index) {
        const int length = static_cast<int>(SendMessageA(list, LB_GETTEXTLEN, index, 0));
        if (length < 0) continue;
        std::vector<char> buffer(static_cast<size_t>(length) + 1, '\0');
        SendMessageA(list, LB_GETTEXT, index, reinterpret_cast<LPARAM>(buffer.data()));
        methods.push_back(buffer.data());
    }
    config["permissions"]["allowed_methods"] = methods;
    config["security"]["origin_allowlist"] = LinesToJsonArray(GetText(hwndDlg, IDC_SECURITY_ORIGINS));
    config["security"]["host_allowlist"] = LinesToJsonArray(GetText(hwndDlg, IDC_SECURITY_HOSTS));
    config["security"]["auth_enabled"] = IsDlgButtonChecked(hwndDlg, IDC_AUTH_ENABLED) == BST_CHECKED;
    config["security"]["auth_token"] = GetText(hwndDlg, IDC_AUTH_TOKEN);
    config["logging"]["enabled"] = IsDlgButtonChecked(hwndDlg, IDC_LOG_ENABLED) == BST_CHECKED;
    const int levelIndex = static_cast<int>(SendMessageA(GetDlgItem(hwndDlg, IDC_LOG_LEVEL), CB_GETCURSEL, 0, 0));
    const char* levels[] = {"debug", "info", "warning", "error"};
    config["logging"]["level"] = levels[levelIndex >= 0 && levelIndex < 4 ? levelIndex : 1];
    config["logging"]["file"] = GetText(hwndDlg, IDC_LOG_FILE);
    config["logging"]["max_file_size_mb"] = GetDlgItemInt(hwndDlg, IDC_LOG_MAX_SIZE, nullptr, FALSE);
    config["logging"]["console_output"] = IsDlgButtonChecked(hwndDlg, IDC_LOG_CONSOLE) == BST_CHECKED;
    config["timeout"]["request_timeout_ms"] = GetDlgItemInt(hwndDlg, IDC_TIMEOUT_REQUEST, nullptr, FALSE);
    config["timeout"]["step_timeout_ms"] = GetDlgItemInt(hwndDlg, IDC_TIMEOUT_STEP, nullptr, FALSE);
    config["timeout"]["memory_read_timeout_ms"] = GetDlgItemInt(hwndDlg, IDC_TIMEOUT_MEMORY, nullptr, FALSE);
    config["features"]["enable_notifications"] = IsDlgButtonChecked(hwndDlg, IDC_FEATURE_NOTIFICATIONS) == BST_CHECKED;
    config["features"]["enable_heartbeat"] = IsDlgButtonChecked(hwndDlg, IDC_FEATURE_HEARTBEAT) == BST_CHECKED;
    config["features"]["heartbeat_interval_seconds"] = GetDlgItemInt(hwndDlg, IDC_HEARTBEAT_INTERVAL, nullptr, FALSE);
    config["features"]["enable_batch_requests"] = IsDlgButtonChecked(hwndDlg, IDC_FEATURE_BATCH) == BST_CHECKED;
    config["features"]["auto_start_mcp_on_plugin_load"] = IsDlgButtonChecked(hwndDlg, IDC_FEATURE_AUTOSTART) == BST_CHECKED;
    config["version"] = config.value("version", "1.0.11");
    return config;
}

bool ConfigEditor::SaveConfig(HWND hwndDlg, const std::string& configPath) {
    json config = GetConfigFromControls(hwndDlg);
    const int port = config["server"]["port"].get<int>();
    if (config["server"]["address"].get<std::string>().empty() || port < 1 || port > 65535) {
        MessageBoxA(hwndDlg, "Enter a valid bind address and TCP port (1-65535).", "MCP Configuration", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (config["security"]["auth_enabled"].get<bool>() && config["security"]["auth_token"].get<std::string>().empty()) {
        MessageBoxA(hwndDlg, "Bearer authentication requires a non-empty token.", "MCP Configuration", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (IsNonLoopbackAddress(config["server"]["address"].get<std::string>()) && !config["security"]["auth_enabled"].get<bool>() &&
        MessageBoxA(hwndDlg, "This listener is not loopback-only and authentication is disabled. Continue?", "Security warning", MB_YESNO | MB_ICONWARNING) != IDYES) {
        return false;
    }
    try {
        std::ofstream file(configPath);
        if (!file.is_open()) throw std::runtime_error("open failed");
        file << config.dump(2);
        s_config = std::move(config);
        Logger::Info("Configuration saved successfully");
        MessageBoxA(hwndDlg, "Configuration saved. Restart the MCP HTTP server to apply it.", "MCP Configuration", MB_OK | MB_ICONINFORMATION);
        return true;
    } catch (const std::exception& e) {
        Logger::Error("Failed to save config: {}", e.what());
        MessageBoxA(hwndDlg, "Failed to save configuration.", "MCP Configuration", MB_OK | MB_ICONERROR);
        return false;
    }
}

} // namespace MCP
