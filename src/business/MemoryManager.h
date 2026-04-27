#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace MCP {

/**
 * @brief 内存区域信息
 */
struct MemoryRegion {
    uint64_t base;
    uint64_t size;
    std::string protection;  // PAGE_EXECUTE_READ等
    std::string type;        // MEM_COMMIT, MEM_RESERVE等
    std::string info;        // 模块名或其他信息
};

/**
 * @brief 内存搜索结果
 */
struct MemorySearchResult {
    uint64_t address;
    std::vector<uint8_t> data;
};

/**
 * @brief 内存管理器
 * 封装 x64dbg 内存访问 API
 */
class MemoryManager {
public:
    /**
     * @brief 获取单例实例
     */
    static MemoryManager& Instance();
    
    /**
     * @brief 读取内存
     * @param address 起始地址
     * @param size 读取大小
     * @return 读取的数据
     */
    std::vector<uint8_t> Read(uint64_t address, size_t size);
    
    /**
     * @brief 写入内存
     * @param address 起始地址
     * @param data 要写入的数据
     * @return 实际写入的字节数
     */
    size_t Write(uint64_t address, const std::vector<uint8_t>& data);
    
    /**
     * @brief 搜索内存模式
     * @param pattern 搜索模式（支持通配符??）
     * @param startAddress 起始地址（0表示全部）
     * @param endAddress 结束地址（0表示全部）
     * @param maxResults 最大结果数
     * @return 搜索结果列表
     */
    std::vector<MemorySearchResult> Search(
        const std::string& pattern,
        uint64_t startAddress = 0,
        uint64_t endAddress = 0,
        size_t maxResults = 1000
    );
    
    /**
     * @brief 获取内存保护属性
     * @param address 地址
     * @return 内存区域信息
     */
    std::optional<MemoryRegion> GetMemoryInfo(uint64_t address);
    
    /**
     * @brief 枚举所有内存区域
     * @return 内存区域列表
     */
    std::vector<MemoryRegion> EnumerateRegions();
    
    /**
     * @brief 验证地址是否可读
     * @param address 地址
     * @param size 大小
     * @return 是否可读
     */
    bool IsReadable(uint64_t address, size_t size);
    
    /**
     * @brief 验证地址是否可写
     * @param address 地址
     * @param size 大小
     * @return 是否可写
     */
    bool IsWritable(uint64_t address, size_t size);
    
    /**
     * @brief 分配内存
     * @param size 大小
     * @return 分配的地址
     */
    uint64_t Allocate(size_t size);
    
    /**
     * @brief 释放内存
     * @param address 地址
     * @return 是否成功
     */
    bool Free(uint64_t address);

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    
    std::vector<uint8_t> ParsePattern(const std::string& pattern);
    bool MatchPattern(const uint8_t* data, const std::vector<uint8_t>& pattern, 
                     const std::vector<bool>& mask);
    std::string ProtectionToString(uint32_t protect);
    
    static constexpr size_t MAX_READ_SIZE = 128 * 1024 * 1024;  // 128 MB
    static constexpr size_t MAX_WRITE_SIZE = 128 * 1024 * 1024;
};

} // namespace MCP
