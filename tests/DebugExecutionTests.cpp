#include "business/DebugController.h"
#include "business/ThreadManager.h"
#include "core/X64DBGBridge.h"
#include "core/Logger.h"
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace {
std::atomic<duint> ip{0x1000};
std::atomic<int> completed{0};
std::atomic<bool> g_running{false};
std::future<void> pending;
}

extern "C" {
bool DbgIsDebugging() { return true; }
bool DbgIsRunning() { return g_running; }
duint DbgValFromString(const char*) { return ip; }
bool DbgMemIsValidReadPtr(duint) { return true; }
HANDLE DbgGetProcessHandle() { return GetCurrentProcess(); }
bool DbgCmdExec(const char*) { return false; } // Steps must not use the queue.
bool DbgCmdExecDirect(const char*) {
    pending = std::async(std::launch::async, [] {
        // The old paused state remains visible until the queued work starts.
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
        g_running = true;
        ++ip;
        ++completed;
        g_running = false;
        MCP::DebugController::Instance().NotifyPaused();
    });
    return true;
}
void GuiUpdateAllViews() {}
HWND GuiGetWindowHandle() { return nullptr; }
}
namespace Script { namespace Module {
bool GetMainModulePath(char*) { return false; }
} }
namespace MCP {
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
ThreadManager& ThreadManager::Instance() { static ThreadManager instance; return instance; }
ThreadInfo ThreadManager::GetCurrentThread() { return {}; }
}

int main() {
    auto& controller = MCP::DebugController::Instance();
    if (controller.StepInto() != 0x1001 || completed != 1) return 1;
    if (controller.StepOver() != 0x1002 || completed != 2) return 2;
    if (controller.StepOut() != 0x1003 || completed != 3) return 3;
    auto first = std::async(std::launch::async, [&] { return controller.StepInto(); });
    auto second = std::async(std::launch::async, [&] { return controller.StepOver(); });
    const auto a = first.get(), b = second.get();
    if (completed != 5 || a == b || ip != 0x1005) return 4;
    pending.get();
    return 0;
}

