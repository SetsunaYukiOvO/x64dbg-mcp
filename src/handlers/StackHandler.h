#pragma once
#include "../core/ResponseBuilder.h"
#include <nlohmann/json.hpp>

namespace MCP {

/**
 * @brief 调用栈处理器
 * 实现调用栈相关的 JSON-RPC 方法
 */
class StackHandler {
public:
    /**
     * @brief 注册所有方法到分发器
     */
    static void RegisterMethods();
    
    /**
     * @brief 获取调用栈回溯
     * Method: stack.get_trace
     * Params: { "max_depth": 100 }  // 可选，默认无限制
     * Returns: { "frames": [ { "address", "from", "to", "comment", ... } ] }
     */
    static nlohmann::json GetStackTrace(const nlohmann::json& params);
    
    /**
     * @brief 读取栈帧数据
     * Method: stack.read_frame
     * Params: { "address": "0x...", "size": 256 }
     * Returns: { "data": "base64...", "address": "0x...", "size": 256 }
     */
    static nlohmann::json ReadStackFrame(const nlohmann::json& params);
    
    /**
     * @brief 获取栈指针信息
     * Method: stack.get_pointers
     * Params: {}
     * Returns architecture-specific stack/base pointer fields ("rsp"/"rbp" or "esp"/"ebp").
     */
    static nlohmann::json GetStackPointers(const nlohmann::json& params);
    
    /**
     * @brief 检查地址是否在栈上
     * Method: stack.is_on_stack
     * Params: { "address": "0x..." }
     * Returns: { "on_stack": true/false }
     */
    static nlohmann::json IsOnStack(const nlohmann::json& params);
    
private:
    /**
     * @brief 格式化栈帧为 JSON
     */
    static nlohmann::json FormatStackFrame(const struct StackFrame& frame);
};

} // namespace MCP
