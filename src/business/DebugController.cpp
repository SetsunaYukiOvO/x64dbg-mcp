#include "DebugController.h"
#include "ThreadManager.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../core/TargetValueValidator.h"
#include "../core/X64DBGBridge.h"
#include <limits>
#include <windows.h>

namespace MCP {

namespace {

bool IsExecutableProtection(DWORD protection) {
    const DWORD access = protection & 0xFF;
    switch (access) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

bool IsRunToTargetAddressValid(uint64_t address) {
    if (address == 0 || !TargetValueValidator::FitsAddress(address)) {
        return false;
    }

    if (!DbgMemIsValidReadPtr(static_cast<duint>(address))) {
        return false;
    }

    HANDLE processHandle = DbgGetProcessHandle();
    if (processHandle == nullptr || processHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi = {};
    SIZE_T queried = VirtualQueryEx(
        processHandle,
        reinterpret_cast<LPCVOID>(address),
        &mbi,
        sizeof(mbi)
    );

    if (queried == 0 || mbi.State != MEM_COMMIT) {
        return false;
    }

    return IsExecutableProtection(mbi.Protect);
}

} // namespace

DebugController& DebugController::Instance() {
    static DebugController instance;
    return instance;
}

DebugState DebugController::GetState() const {
    if (!DbgIsDebugging()) {
        return DebugState::Stopped;
    }
    
    if (DbgIsRunning()) {
        return DebugState::Running;
    }
    
    return DebugState::Paused;
}

uint64_t DebugController::GetInstructionPointer() const {
    if (!IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 使用 x64dbg API 获取 RIP/EIP
    duint rip = DbgValFromString("cip");
    if (rip != 0) {
        return static_cast<uint64_t>(rip);
    }

    // Fallback to thread context so debug_get_state can report a stable IP.
    try {
        const ThreadInfo currentThread = ThreadManager::Instance().GetCurrentThread();
        if (currentThread.rip != 0) {
            Logger::Trace("GetInstructionPointer fallback to thread RIP: 0x{:X}", currentThread.rip);
            return currentThread.rip;
        }
    } catch (const std::exception& e) {
        Logger::Trace("GetInstructionPointer fallback failed: {}", e.what());
    }

    return 0;
}

bool DebugController::Run() {
    if (!IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    Logger::Debug("Executing run command");
    return ExecuteCommand("run");
}

bool DebugController::Pause() {
    if (!IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    if (IsPaused()) {
        Logger::Warning("Debugger is already paused");
        return true;
    }
    
    bool breakRequested = false;

#ifdef XDBG_SDK_AVAILABLE
    // Force an async break first; command-only pause may not interrupt long-running loops.
    HANDLE processHandle = DbgGetProcessHandle();
    if (processHandle != nullptr && processHandle != INVALID_HANDLE_VALUE) {
        if (DebugBreakProcess(processHandle) != FALSE) {
            breakRequested = true;
            Logger::Debug("Pause requested via DebugBreakProcess");
        } else {
            Logger::Warning("DebugBreakProcess failed with error {}", GetLastError());
        }
    }
#endif

    if (!breakRequested) {
        Logger::Debug("Executing pause command");
        breakRequested = ExecuteCommand("pause");
    }

    if (!breakRequested) {
        return false;
    }

    return WaitForPause(5000);
}

uint64_t DebugController::StepInto() {
    if (!IsPaused()) {
        throw DebuggerNotPausedException();
    }
    
    Logger::Debug("Executing step into");
    
    if (!ExecuteCommand("sti")) {
        throw MCPException("Step into failed");
    }
    
    if (!WaitForPause()) {
        throw OperationTimeoutException("Step into timeout");
    }
    
    // 给 x64dbg 一点时间更新寄存器状态
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    return GetInstructionPointer();
}

uint64_t DebugController::StepOver() {
    if (!IsPaused()) {
        throw DebuggerNotPausedException();
    }
    
    Logger::Debug("Executing step over");
    
    if (!ExecuteCommand("sto")) {
        throw MCPException("Step over failed");
    }
    
    if (!WaitForPause()) {
        throw OperationTimeoutException("Step over timeout");
    }
    
    // 给 x64dbg 一点时间更新寄存器状态
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    return GetInstructionPointer();
}

uint64_t DebugController::StepOut() {
    if (!IsPaused()) {
        throw DebuggerNotPausedException();
    }
    
    Logger::Debug("Executing step out");
    
    if (!ExecuteCommand("rtr")) {
        throw MCPException("Step out failed");
    }
    
    if (!WaitForPause()) {
        throw OperationTimeoutException("Step out timeout");
    }
    
    // 给 x64dbg 一点时间更新寄存器状态
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    return GetInstructionPointer();
}

bool DebugController::RunToAddress(uint64_t address) {
    if (!IsPaused()) {
        throw DebuggerNotPausedException();
    }

    if (!IsRunToTargetAddressValid(address)) {
        Logger::Warning("RunToAddress rejected invalid target: 0x{:X}", address);
        return false;
    }

    // If we're already at the target address, avoid unnecessary execution.
    if (GetInstructionPointer() == address) {
        Logger::Debug("RunToAddress target already reached: 0x{:X}", address);
        return true;
    }
    
    char command[64];
    sprintf_s(command, "rtu %llX", address);
    
    Logger::Debug("Executing run to address: 0x{:X}", address);
    if (!ExecuteCommand(command)) {
        return false;
    }

    if (!WaitForPause(10000)) {
        Logger::Warning("RunToAddress timed out waiting for pause at target 0x{:X}", address);
        return false;
    }

    const uint64_t rip = GetInstructionPointer();
    if (rip != address) {
        Logger::Warning(
            "RunToAddress paused at unexpected address. target=0x{:X}, current=0x{:X}",
            address,
            rip
        );
        return false;
    }

    return true;
}

bool DebugController::Restart() {
    // x64dbg has no "restart" script command. The GUI's Restart action
    // executes `init "<last debugged file>"` (see x64dbg's
    // MainWindow::restartDebugging). Reproduce that here.
    //
    // When a session is active we can resolve the path from the currently
    // loaded main module. When the debuggee has already exited (crash / exit
    // process / user-initiated stop) we fall back to the last path cached by
    // a previous Init/Restart so the bot can re-launch without knowing the
    // original path.
    std::string resolvedPath;

    if (IsDebugging()) {
        char path[MAX_PATH] = {};
        if (Script::Module::GetMainModulePath(path) && path[0] != '\0') {
            resolvedPath.assign(path);
        }
    }

    if (resolvedPath.empty()) {
        resolvedPath = LoadLastDebuggedPath();
    }

    if (resolvedPath.empty()) {
        Logger::Error("Restart failed: no debuggee path available (session inactive and cache empty)");
        return false;
    }

    return Init(resolvedPath);
}

bool DebugController::Init(const std::string& path,
                           const std::string& arguments,
                           const std::string& currentDir) {
    std::string resolvedPath = path;
    if (resolvedPath.empty()) {
        resolvedPath = LoadLastDebuggedPath();
    }

    if (resolvedPath.empty()) {
        Logger::Error("Init failed: no executable path provided and no cached path available");
        return false;
    }

    // Build `init "<path>"[, "<args>"[, "<wdir>"]]`. x64dbg's command parser
    // splits arguments on commas, so quoting each component keeps paths with
    // spaces or commas intact. Escape embedded quotes to prevent command injection.
    auto escapeQuotes = [](const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '"') out += '\\';
            out += c;
        }
        return out;
    };

    std::string command;
    command.reserve(resolvedPath.size() + arguments.size() + currentDir.size() + 32);
    command.append("init \"");
    command.append(escapeQuotes(resolvedPath));
    command.append("\"");

    if (!arguments.empty()) {
        command.append(", \"");
        command.append(escapeQuotes(arguments));
        command.append("\"");
    }

    if (!currentDir.empty()) {
        if (arguments.empty()) {
            command.append(", \"\"");
        }
        command.append(", \"");
        command.append(escapeQuotes(currentDir));
        command.append("\"");
    }

    Logger::Debug("Starting debugger via: {}", command);
    const bool success = ExecuteCommand(command);
    if (success) {
        CacheLastDebuggedPath(resolvedPath);
    }
    return success;
}

bool DebugController::DetachProcessCore() {
    if (!IsDebugging()) {
        return true;
    }
    Logger::Debug("DetachProcessCore: detach/stop");
    ExecuteCommandDirect("detach");
    Sleep(200);
    if (IsDebugging()) {
        ExecuteCommandDirect("stop");
        Sleep(200);
    }
    return !IsDebugging();
}

bool DebugController::AttachProcessCore(uint32_t pid, bool useAttachBreak, bool detachFirst) {
    if (pid == 0) {
        return false;
    }

    if (IsDebugging()) {
        const uint32_t currentPid = GetDebuggeeProcessId();
        if (currentPid == pid) {
            Logger::Info("AttachProcessCore: already on pid {}", pid);
            return true;
        }
        if (!detachFirst) {
            Logger::Error("AttachProcessCore: busy with pid {}, want {}", currentPid, pid);
            return false;
        }
        if (!DetachProcessCore()) {
            Logger::Error("AttachProcessCore: detach failed");
            return false;
        }
    }

    // x64dbg 命令行整数默认为十六进制；十进制 PID 必须用 ".1234" 形式（见 Values 文档）
    // x64dbg 没有 attachbreak 命令；useAttachBreak 仅设置断点等待标记，
    // 实际的暂停等待由 AttachProcess 中的 WaitForPause 完成。
    char command[64] = {};
    sprintf_s(command, "attach .%u", pid);
    (void)useAttachBreak;  // honored by caller AttachProcess via WaitForPause

    Logger::Info("AttachProcessCore: {} (decimal pid)", command);
    if (!ExecuteCommandDirect(command)) {
        Logger::Error("AttachProcessCore: direct command failed");
        return false;
    }

    return IsDebugging();
}

bool DebugController::AttachProcess(uint32_t pid,
                                    uint32_t timeoutMs,
                                    bool useAttachBreak,
                                    bool detachIfBusy) {
    if (pid == 0) {
        Logger::Error("AttachProcess: invalid pid 0");
        return false;
    }

    // 轻量校验：存在即可；真正 OpenProcess(PROCESS_ALL_ACCESS) 在 x64dbg attach 命令内完成
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (processHandle == nullptr || processHandle == INVALID_HANDLE_VALUE) {
        processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    }
    if (processHandle == nullptr || processHandle == INVALID_HANDLE_VALUE) {
        Logger::Warning(
            "AttachProcess: OpenProcess probe failed for pid {} (error {}), trying attach anyway",
            pid,
            GetLastError()
        );
    } else {
        DWORD exitCode = STILL_ACTIVE;
        if (GetExitCodeProcess(processHandle, &exitCode) && exitCode != STILL_ACTIVE) {
            CloseHandle(processHandle);
            Logger::Error("AttachProcess: pid {} already exited (code {})", pid, exitCode);
            return false;
        }
        CloseHandle(processHandle);
    }

    if (IsDebugging()) {
        const uint32_t currentPid = GetDebuggeeProcessId();
        if (currentPid == pid) {
            Logger::Info("AttachProcess: already attached to pid {}", pid);
            return true;
        }
        if (!detachIfBusy) {
            Logger::Error("AttachProcess: already debugging pid {}, requested {}", currentPid, pid);
            return false;
        }
        Logger::Debug("AttachProcess: detaching before attach");
        if (!ExecuteCommand("mcpdetach")) {
            Logger::Error("AttachProcess: mcpdetach enqueue failed");
            return false;
        }
        const uint32_t waitMs = timeoutMs > 0 ? timeoutMs : 15000;
        auto start = std::chrono::steady_clock::now();
        while (IsDebugging()) {
            PumpGuiMessages();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= waitMs) {
                Logger::Error("AttachProcess: detach timed out");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    char pluginCmd[32] = {};
    sprintf_s(pluginCmd, "mcpattach %u", pid);
    (void)useAttachBreak;
    Logger::Info("AttachProcess: queue {}", pluginCmd);
    if (!ExecuteCommand(pluginCmd)) {
        Logger::Error("AttachProcess: plugin command enqueue failed");
        return false;
    }

    const uint32_t waitMs = timeoutMs > 0 ? timeoutMs : 30000;
    if (!WaitForDebugging(waitMs)) {
        Logger::Error("AttachProcess: DbgIsDebugging still false after {} ms", waitMs);
        return false;
    }

    // attach 后通常在系统断点暂停；attach_break 仅表示多等一等暂停态
    if (useAttachBreak && !WaitForPause(waitMs)) {
        Logger::Warning(
            "AttachProcess: WaitForPause timed out after {} ms (running={})",
            waitMs,
            DbgIsRunning()
        );
    }

    const uint32_t attachedPid = GetDebuggeeProcessId();
    if (attachedPid != 0 && attachedPid != pid) {
        Logger::Warning("AttachProcess: requested pid {} but debugger reports pid {}", pid, attachedPid);
    }

    try {
        Logger::Info(
            "AttachProcess: ok pid={} rip=0x{:X} state={}",
            attachedPid != 0 ? attachedPid : pid,
            GetInstructionPointer(),
            static_cast<int>(GetState())
        );
    } catch (...) {
        Logger::Info("AttachProcess: ok pid={}", attachedPid != 0 ? attachedPid : pid);
    }

    return true;
}

uint32_t DebugController::GetDebuggeeProcessId() const {
    if (!IsDebugging()) {
        return 0;
    }

    duint pid = DbgValFromString("$pid");
    if (pid == 0) {
        pid = DbgValFromString("pid");
    }
    if (pid == 0 || pid > std::numeric_limits<uint32_t>::max()) {
        return 0;
    }
    return static_cast<uint32_t>(pid);
}

std::string DebugController::GetLastDebuggedPath() const {
    return LoadLastDebuggedPath();
}

void DebugController::NotifyDebugSessionStarted(const std::string& path) {
    if (!path.empty()) {
        CacheLastDebuggedPath(path);
    }
}

void DebugController::CacheLastDebuggedPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_lastPathMutex);
    m_lastDebuggedPath = path;
}

std::string DebugController::LoadLastDebuggedPath() const {
    std::lock_guard<std::mutex> lock(m_lastPathMutex);
    return m_lastDebuggedPath;
}

bool DebugController::Stop() {
    if (!IsDebugging()) {
        Logger::Warning("Debugger is not running");
        return true;
    }
    
    Logger::Debug("Stopping debugger");
    return ExecuteCommand("stop");
}

bool DebugController::IsDebugging() const {
    return DbgIsDebugging();
}

bool DebugController::IsPaused() const {
    return IsDebugging() && !DbgIsRunning();
}

bool DebugController::ExecuteCommand(const std::string& command) {
    Logger::Trace("Executing command: {}", command);
    
    bool result = DbgCmdExec(command.c_str());
    
    if (!result) {
        Logger::Error("Command failed: {}", command);
    }
    
    return result;
}

bool DebugController::ExecuteCommandDirect(const std::string& command) {
    Logger::Trace("Executing command (direct): {}", command);

    bool result = DbgCmdExecDirect(command.c_str());

    if (!result) {
        Logger::Error("Direct command failed: {}", command);
    }

    return result;
}

void DebugController::PumpGuiMessages() {
#ifdef XDBG_SDK_AVAILABLE
    // CAUTION: This is called from HTTP worker threads during attach polling.
    // DbgUpdateGui is documented as thread-safe by the x64dbg SDK. PeekMessage
    // with nullptr HWND dispatches only messages for the calling thread, which
    // is fine for interop with x64dbg's command queue processing.
    DbgUpdateGui(0, false);

    HWND hwnd = GuiGetWindowHandle();
    if (hwnd == nullptr) {
        return;
    }

    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}

bool DebugController::WaitForDebugging(uint32_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        PumpGuiMessages();

        if (IsDebugging()) {
            return true;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

        if (elapsed >= timeoutMs) {
            Logger::Warning("Wait for debugging timed out after {} ms", timeoutMs);
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool DebugController::WaitForPause(uint32_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        PumpGuiMessages();

        if (IsPaused()) {
            return true;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        
        if (elapsed >= timeoutMs) {
            Logger::Warning("Wait for pause timed out after {} ms", timeoutMs);
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace MCP
