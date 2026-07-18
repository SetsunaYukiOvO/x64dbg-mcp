#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace MCP {

/**
 * @brief 断点类型
 */
enum class BreakpointType {
    Software,    // 软件断点 (INT3)
    Hardware,    // 硬件断点 (DR0-DR3)
    Memory       // 内存断点 (页面保护)
};

/**
 * @brief 硬件断点条件
 */
enum class HardwareBreakpointCondition {
    Execute,     // 执行时触发
    Write,       // 写入时触发
    ReadWrite    // 读写时触发
};

/**
 * @brief 硬件断点大小
 */
enum class HardwareBreakpointSize {
    Byte1 = 1,
    Byte2 = 2,
    Byte4 = 4,
    Byte8 = 8
};

constexpr bool IsHardwareBreakpointSizeSupported(HardwareBreakpointSize size) {
    switch (size) {
        case HardwareBreakpointSize::Byte1:
        case HardwareBreakpointSize::Byte2:
        case HardwareBreakpointSize::Byte4:
            return true;
        case HardwareBreakpointSize::Byte8:
#ifdef XDBG_ARCH_X64
            return true;
#else
            return false;
#endif
    }
    return false;
}

constexpr bool IsHardwareBreakpointRequestValid(
    uint64_t address,
    HardwareBreakpointCondition condition,
    HardwareBreakpointSize size,
    uint64_t maximumAddress = std::numeric_limits<uint64_t>::max())
{
    return address <= maximumAddress &&
           IsHardwareBreakpointSizeSupported(size) &&
           (condition != HardwareBreakpointCondition::Execute || size == HardwareBreakpointSize::Byte1) &&
           address % static_cast<uint64_t>(size) == 0;
}

/**
 * @brief 断点信息
 */
struct BreakpointInfo {
    uint64_t address;
    BreakpointType type;
    bool enabled;
    std::string name;           // 可选的断点名称
    std::string module;         // 所属模块
    uint32_t hitCount;          // 命中次数
    
    // 硬件断点特有属性
    HardwareBreakpointCondition condition;
    HardwareBreakpointSize size;
    size_t memorySize;
    
    // 条件断点
    std::string condition_expr;  // 条件表达式
    
    // 日志断点
    bool isLogBreakpoint;
    std::string logMessage;
};

/**
 * @brief 断点管理器
 * 封装 x64dbg 断点操作 API
 */
class BreakpointManager {
public:
    /**
     * @brief 获取单例实例
     */
    static BreakpointManager& Instance();
    
    /**
     * @brief 设置软件断点
     * @param address 断点地址
     * @param name 断点名称 (可选)
     * @return 是否成功
     */
    bool SetSoftwareBreakpoint(uint64_t address, const std::string& name = "");
    
    /**
     * @brief 设置硬件断点
     * @param address 断点地址
     * @param condition 触发条件
     * @param size 监控大小
     * @param name 断点名称 (可选)
     * @return 是否成功
     */
    bool SetHardwareBreakpoint(
        uint64_t address,
        HardwareBreakpointCondition condition,
        HardwareBreakpointSize size,
        const std::string& name = ""
    );
    
    /**
     * @brief 设置内存断点
     * @param address 断点地址
     * @param size 监控大小
     * @param name 断点名称 (可选)
     * @return 是否成功
     */
    bool SetMemoryBreakpoint(uint64_t address, size_t size, const std::string& name = "");
    
    /**
     * @brief 删除断点
     * @param address 断点地址
     * @param type 断点类型 (可选,如果不指定则删除该地址的所有断点)
     * @return 是否成功
     */
    bool DeleteBreakpoint(uint64_t address, std::optional<BreakpointType> type = std::nullopt);
    
    /**
     * @brief 启用断点
     * @param address 断点地址
     * @return 是否成功
     */
    bool EnableBreakpoint(uint64_t address);
    
    /**
     * @brief 禁用断点
     * @param address 断点地址
     * @return 是否成功
     */
    bool DisableBreakpoint(uint64_t address);
    
    /**
     * @brief 切换断点状态
     * @param address 断点地址
     * @return 是否成功
     */
    bool ToggleBreakpoint(uint64_t address);
    
    /**
     * @brief 获取断点信息
     * @param address 断点地址
     * @return 断点信息
     */
    std::optional<BreakpointInfo> GetBreakpoint(uint64_t address);
    
    /**
     * @brief 列出所有断点
     * @param typeFilter 类型过滤器 (可选)
     * @return 断点列表
     */
    std::vector<BreakpointInfo> ListBreakpoints(std::optional<BreakpointType> typeFilter = std::nullopt);
    
    /**
     * @brief 删除所有断点
     * @param typeFilter 类型过滤器 (可选)
     * @return 删除的断点数量
     */
    size_t DeleteAllBreakpoints(std::optional<BreakpointType> typeFilter = std::nullopt);
    
    /**
     * @brief 设置条件断点
     * @param address 断点地址
     * @param condition 条件表达式
     * @return 是否成功
     */
    bool SetCondition(uint64_t address, const std::string& condition);
    
    /**
     * @brief 设置日志断点
     * @param address 断点地址
     * @param message 日志消息
     * @return 是否成功
     */
    bool SetLogBreakpoint(uint64_t address, const std::string& message);
    
    /**
     * @brief 重命名断点
     * @param address 断点地址
     * @param name 新名称
     * @return 是否成功
     */
    bool RenameBreakpoint(uint64_t address, const std::string& name);
    
    /**
     * @brief 检查地址是否有断点
     * @param address 地址
     * @return 是否有断点
     */
    bool HasBreakpoint(uint64_t address);
    
    /**
     * @brief 获取断点命中次数
     * @param address 断点地址
     * @return 命中次数
     */
    uint32_t GetHitCount(uint64_t address);
    
    /**
     * @brief 重置命中次数
     * @param address 断点地址
     * @return 是否成功
     */
    bool ResetHitCount(uint64_t address);

private:
    BreakpointManager() = default;
    ~BreakpointManager() = default;
    BreakpointManager(const BreakpointManager&) = delete;
    BreakpointManager& operator=(const BreakpointManager&) = delete;
    
    std::string BreakpointTypeToString(BreakpointType type);
    std::string HardwareConditionToString(HardwareBreakpointCondition condition);
    std::string GetModuleName(uint64_t address);
};

} // namespace MCP
