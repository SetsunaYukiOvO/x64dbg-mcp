#pragma once
#include <string>
#include <cstdint>
#include <mutex>

namespace MCP {

/**
 * @brief 调试器状态
 */
enum class DebugState {
    Stopped,    // 未运行
    Running,    // 正在运行
    Paused      // 已暂停
};

/**
 * @brief 调试控制器
 * 封装 x64dbg 调试控制 API
 */
class DebugController {
public:
    /**
     * @brief 获取单例实例
     */
    static DebugController& Instance();
    
    /**
     * @brief 获取调试器状态
     */
    DebugState GetState() const;
    
    /**
     * @brief 获取当前指令指针
     */
    uint64_t GetInstructionPointer() const;
    
    /**
     * @brief 继续执行
     * @return 是否成功
     */
    bool Run();
    
    /**
     * @brief 暂停执行
     * @return 是否成功
     */
    bool Pause();
    
    /**
     * @brief 单步进入
     * @return 新的指令指针
     */
    uint64_t StepInto();
    
    /**
     * @brief 单步跨越
     * @return 新的指令指针
     */
    uint64_t StepOver();
    
    /**
     * @brief 执行到返回
     * @return 新的指令指针
     */
    uint64_t StepOut();
    
    /**
     * @brief 执行到指定地址
     * @param address 目标地址
     * @return 是否成功
     */
    bool RunToAddress(uint64_t address);
    
    /**
     * @brief 重启调试
     * @return 是否成功
     */
    bool Restart();

    /**
     * @brief 启动调试（加载并运行目标程序）
     *
     * Mirrors the behaviour of x64dbg's "Run" button when no session is
     * active: invokes the `init` script command to load and start the given
     * executable. Works regardless of current debug state (e.g. after the
     * debuggee crashed or exited).
     *
     * @param path 可执行文件路径（为空时回退到上次缓存的路径）
     * @param arguments 命令行参数（可选）
     * @param currentDir 工作目录（可选）
     * @return 是否成功
     */
    bool Init(const std::string& path,
              const std::string& arguments = "",
              const std::string& currentDir = "");

    /**
     * @brief 获取上次加载过的调试目标路径
     */
    std::string GetLastDebuggedPath() const;

    /**
     * @brief 记录调试目标路径（由调试会话回调调用，不会触发启动）
     */
    void NotifyDebugSessionStarted(const std::string& path);

    /**
     * @brief 停止调试
     * @return 是否成功
     */
    bool Stop();
    
    /**
     * @brief 检查调试器是否正在运行
     */
    bool IsDebugging() const;
    
    /**
     * @brief 检查调试器是否已暂停
     */
    bool IsPaused() const;

private:
    DebugController() = default;
    ~DebugController() = default;
    DebugController(const DebugController&) = delete;
    DebugController& operator=(const DebugController&) = delete;
    
    bool ExecuteCommand(const std::string& command);
    bool WaitForPause(uint32_t timeoutMs = 5000);

    mutable std::mutex m_lastPathMutex;
    std::string m_lastDebuggedPath;

    void CacheLastDebuggedPath(const std::string& path);
    std::string LoadLastDebuggedPath() const;
};

} // namespace MCP
