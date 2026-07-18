#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <limits>

namespace MCP {

constexpr bool IsWithinEstimatedStackRange(
    uint64_t address,
    uint64_t stackPointer,
    uint64_t maximumAddress = std::numeric_limits<uint64_t>::max())
{
    constexpr uint64_t kLowerEstimate = 0x10000;
    constexpr uint64_t kUpperEstimate = 1024 * 1024;
    if (address > maximumAddress || stackPointer > maximumAddress) {
        return false;
    }
    const uint64_t lowerBound = stackPointer >= kLowerEstimate
        ? stackPointer - kLowerEstimate
        : std::numeric_limits<uint64_t>::min();
    const uint64_t upperBound = maximumAddress - stackPointer >= kUpperEstimate
        ? stackPointer + kUpperEstimate
        : maximumAddress;
    return address >= lowerBound && address <= upperBound;
}

/**
 * @brief 调用栈帧信息
 */
struct StackFrame {
    uint64_t address;           // 返回地址
    uint64_t from;              // 调用源地址
    uint64_t to;                // 调用目标地址
    std::string comment;        // 注释（函数名或符号）
    uint64_t party;             // 模块标识
    bool isUser;                // 是否用户代码
    
    // 寄存器上下文（可选）
    uint64_t rsp;
    uint64_t rbp;
};

/**
 * @brief 栈管理器
 * 封装 x64dbg 调用栈分析 API
 */
class StackManager {
public:
    /**
     * @brief 获取单例实例
     */
    static StackManager& Instance();
    
    /**
     * @brief 获取当前调用栈回溯
     * @param maxDepth 最大深度（0表示无限制）
     * @return 调用栈帧列表（从内层到外层）
     */
    std::vector<StackFrame> GetStackTrace(size_t maxDepth = 0);
    
    /**
     * @brief 读取栈帧数据
     * @param frameAddress 栈帧地址（RSP或RBP）
     * @param size 读取大小（字节）
     * @return 栈数据
     */
    std::vector<uint8_t> ReadStackFrame(uint64_t frameAddress, size_t size);
    
    /**
     * @brief 获取栈顶地址（RSP）
     * @return 当前栈顶地址
     */
    uint64_t GetStackPointer();
    
    /**
     * @brief 获取栈底地址（RBP）
     * @return 当前栈底地址
     */
    uint64_t GetBasePointer();
    
    /**
     * @brief 检查地址是否在栈范围内
     * @param address 待检查地址
     * @return 是否在栈范围
     */
    bool IsAddressOnStack(uint64_t address);
    
private:
    StackManager() = default;
    ~StackManager() = default;
    StackManager(const StackManager&) = delete;
    StackManager& operator=(const StackManager&) = delete;
    
    /**
     * @brief 解析栈帧符号信息
     * @param address 地址
     * @return 符号注释
     */
    std::string ResolveSymbol(uint64_t address);
    
    /**
     * @brief 手动栈回溯实现
     * @param maxDepth 最大深度
     * @return 调用栈帧列表
     */
    std::vector<StackFrame> GetStackTraceManual(size_t maxDepth);
};

} // namespace MCP
