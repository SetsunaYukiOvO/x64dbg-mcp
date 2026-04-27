#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>

namespace MCP {

/**
 * @brief Dump选项
 */
struct DumpOptions {
    bool fixOEP = true;             // 修复入口点 (OEP)
    bool removeIntegrityCheck = true; // 移除PE校验和
    bool rebuildPE = true;          // 重建PE头
    bool autoDetectOEP = false;     // 自动检测OEP
    bool dumpFullImage = false;     // dump完整镜像(包括未映射部分)
    std::optional<uint64_t> forcedOEP;
};

/**
 * @brief Dump进度信息
 */
struct DumpProgress {
    enum class Stage {
        Preparing,              // 准备阶段
        ReadingMemory,          // 读取内存
        FixingPEHeaders,        // 修复PE头
        Writing,                // 写入文件
        Completed,              // 完成
        Failed                  // 失败
    };
    
    Stage stage = Stage::Preparing;
    int progress = 0;           // 0-100
    std::string message;
    bool success = false;
};

/**
 * @brief Dump结果
 */
struct DumpResult {
    bool success = false;
    std::string filePath;
    uint64_t dumpedSize = 0;
    uint64_t originalEP = 0;    // 原始入口点
    uint64_t newEP = 0;         // 新入口点
    std::string error;
    DumpProgress finalProgress;
};

/**
 * @brief 模块Dump信息
 */
struct ModuleDumpInfo {
    std::string name;
    std::string path;
    uint64_t baseAddress;
    uint64_t size;
    uint64_t entryPoint;
    bool isPacked = false;      // 是否加壳
    std::string packerId;       // 壳类型识别
};

/**
 * @brief 内存区域Dump信息
 */
struct MemoryRegionDump {
    uint64_t address;
    uint64_t size;
    std::string protection;
    std::string type;
    std::string name;           // 相关模块名或描述
};

/**
 * @brief Dump管理器
 * 提供自动化脱壳和内存dump功能
 */
class DumpManager {
public:
    using ProgressCallback = std::function<void(const DumpProgress&)>;
    
    /**
     * @brief 获取单例实例
     */
    static DumpManager& Instance();
    
    /**
     * @brief Dump指定模块到文件
     * @param moduleNameOrAddress 模块名或基址
     * @param outputPath 输出文件路径
     * @param options Dump选项
     * @param progressCallback 进度回调(可选)
     * @return Dump结果
     */
    DumpResult DumpModule(
        const std::string& moduleNameOrAddress,
        const std::string& outputPath,
        const DumpOptions& options = DumpOptions(),
        ProgressCallback progressCallback = nullptr
    );
    
    /**
     * @brief Dump指定内存区域到文件
     * @param startAddress 起始地址
     * @param size 大小
     * @param outputPath 输出文件路径
     * @param asRawBinary 是否以原始二进制格式保存(否则尝试修复PE)
     * @return Dump结果
     */
    DumpResult DumpMemoryRegion(
        uint64_t startAddress,
        size_t size,
        const std::string& outputPath,
        bool asRawBinary = false
    );
    /**
     * @brief 分析模块是否加壳
     * @param moduleNameOrAddress 模块名或基址
     * @return 模块信息(包含加壳检测结果)
     */
    ModuleDumpInfo AnalyzeModule(const std::string& moduleNameOrAddress);
    
    /**
     * @brief 检测当前OEP(Original Entry Point)
     * @param moduleBase 模块基址
     * @param strategy 检测策略: "entropy", "code_analysis", "api_calls", "tls"
     * @return OEP地址,如果未找到返回nullopt
     */
    std::optional<uint64_t> DetectOEP(uint64_t moduleBase, const std::string& strategy = "code_analysis");
    
    /**
     * @brief 获取可dump的内存区域列表
     * @param moduleBase 模块基址(可选,0表示所有)
     * @return 内存区域列表
     */
    std::vector<MemoryRegionDump> GetDumpableRegions(uint64_t moduleBase = 0);
    /**
     * @brief 重建PE头
     * @param moduleBase 模块基址
     * @param buffer PE文件缓冲区(会被修改)
     * @param newEP 新入口点RVA(可选)
     * @return 是否成功
     */
    bool RebuildPEHeaders(uint64_t moduleBase, std::vector<uint8_t>& buffer,
                         std::optional<uint32_t> newEP = std::nullopt);
    /**
     * @brief 设置OEP检测策略
     * @param strategy 策略函数: (moduleBase) -> OEP地址
     */
    void SetOEPDetectionStrategy(
        std::function<std::optional<uint64_t>(uint64_t)> strategy
    );
    
    /**
     * @brief 解析模块名或地址
     * @param input 模块名或地址字符串
     * @return 模块基址，如果无效返回 nullopt
     */
    std::optional<uint64_t> ParseModuleOrAddress(const std::string& input);

private:
    DumpManager() = default;
    ~DumpManager() = default;
    DumpManager(const DumpManager&) = delete;
    DumpManager& operator=(const DumpManager&) = delete;
    
    // 内部辅助方法
    bool ValidatePEHeader(const std::vector<uint8_t>& buffer);
    bool IsPacked(uint64_t moduleBase, std::string& packerId);
    uint64_t GetModuleSize(uint64_t moduleBase);
    uint64_t GetModuleEntryPoint(uint64_t moduleBase);
    std::string GetModulePath(uint64_t moduleBase);
    
    // OEP检测辅助
    std::optional<uint64_t> DetectOEPByPattern(uint64_t moduleBase);
    // PE修复辅助
    bool FixPEChecksum(std::vector<uint8_t>& buffer);
    bool AlignPESections(std::vector<uint8_t>& buffer);
    // 用户自定义OEP检测策略
    std::function<std::optional<uint64_t>(uint64_t)> m_oepDetectionStrategy;
};

} // namespace MCP
