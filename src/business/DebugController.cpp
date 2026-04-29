#include "DebugController.h"
#include "ThreadManager.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../core/X64DBGBridge.h"
#include <limits>

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
    if (address == 0 || address > std::numeric_limits<duint>::max()) {
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
    if (!IsDebugging()) {
        throw DebuggerNotRunningException();
    }

    // x64dbg has no "restart" script command. The GUI's Restart action
    // executes `init "<last debugged file>"` (see x64dbg's
    // MainWindow::restartDebugging). Reproduce that here using the
    // currently debugged main module path.
    char path[MAX_PATH] = {};
    if (!Script::Module::GetMainModulePath(path) || path[0] == '\0') {
        Logger::Error("Restart failed: unable to resolve main module path");
        return false;
    }

    std::string command;
    command.reserve(MAX_PATH + 8);
    command.append("init \"");
    command.append(path);
    command.append("\"");

    Logger::Debug("Restarting debugger via: {}", command);
    return ExecuteCommand(command);
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

bool DebugController::WaitForPause(uint32_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
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
