#pragma once
#include "../business/DebugController.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace MCP {

/**
 * @brief 调试方法处理器
 * 实现 debug.* 系列 JSON-RPC 方法
 */
class DebugHandler {
public:
    /**
     * @brief 注册所有调试方法到分发器
     */
    static void RegisterMethods();
    
    /**
     * @brief debug.get_state - 获取调试器状态
     */
    static json GetState(const json& params);
    
    /**
     * @brief debug.run - 继续执行
     */
    static json Run(const json& params);
    
    /**
     * @brief debug.pause - 暂停执行
     */
    static json Pause(const json& params);
    
    /**
     * @brief debug.step_into - 单步进入
     */
    static json StepInto(const json& params);
    
    /**
     * @brief debug.step_over - 单步跨越
     */
    static json StepOver(const json& params);
    
    /**
     * @brief debug.step_out - 执行到返回
     */
    static json StepOut(const json& params);
    
    /**
     * @brief debug.run_to - 执行到指定地址
     */
    static json RunTo(const json& params);
    
    /**
     * @brief debug.restart - 重启调试
     */
    static json Restart(const json& params);

    /**
     * @brief debug.init - 启动（或重新启动）目标程序
     *
     * Corresponds to x64dbg's "Run" button: loads the given executable and
     * begins a new debug session. Works even when no session is currently
     * active — useful when the previous debuggee has exited or crashed.
     */
    static json Init(const json& params);

    /**
     * @brief debug.stop - 停止调试
     */
    static json Stop(const json& params);

private:
    static std::string StateToString(DebugState state);
};

} // namespace MCP
