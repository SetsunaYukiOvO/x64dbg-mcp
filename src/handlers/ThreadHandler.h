#pragma once
#include "../core/ResponseBuilder.h"
#include <nlohmann/json.hpp>

namespace MCP {

/**
 * @brief 线程处理器
 * 实现线程管理相关的 JSON-RPC 方法
 */
class ThreadHandler {
public:
    /**
     * @brief 注册所有方法到分发器
     */
    static void RegisterMethods();
    
    /**
     * @brief 获取所有线程列表
     * Method: thread.list
     * Params: {}
     * Returns: { "threads": [ { "id", "name", "entry", ... } ], "count": 5 }
     */
    static nlohmann::json ListThreads(const nlohmann::json& params);
    
    /**
     * @brief 获取当前线程信息
     * Method: thread.get_current
     * Params: {}
     * Returns an architecture-specific instruction pointer field ("rip" or "eip").
     */
    static nlohmann::json GetCurrentThread(const nlohmann::json& params);
    
    /**
     * @brief 获取指定线程信息
     * Method: thread.get
     * Params: { "thread_id": 1234 }
     * Returns: { "id", "name", "entry", ... }
     */
    static nlohmann::json GetThread(const nlohmann::json& params);
    
    /**
     * @brief 切换到指定线程
     * Method: thread.switch
     * Params: { "thread_id": 1234 }
     * Returns: { "success": true, "previous_id": 5678, "current_id": 1234 }
     */
    static nlohmann::json SwitchThread(const nlohmann::json& params);
    
    /**
     * @brief 挂起线程
     * Method: thread.suspend
     * Params: { "thread_id": 1234 }
     * Returns: { "success": true }
     */
    static nlohmann::json SuspendThread(const nlohmann::json& params);
    
    /**
     * @brief 恢复线程
     * Method: thread.resume
     * Params: { "thread_id": 1234 }
     * Returns: { "success": true }
     */
    static nlohmann::json ResumeThread(const nlohmann::json& params);
    
    /**
     * @brief 获取线程数量
     * Method: thread.get_count
     * Params: {}
     * Returns: { "count": 5 }
     */
    static nlohmann::json GetThreadCount(const nlohmann::json& params);
    
private:
    /**
     * @brief 格式化线程信息为 JSON
     */
    static nlohmann::json FormatThreadInfo(const struct ThreadInfo& info);
};

} // namespace MCP
