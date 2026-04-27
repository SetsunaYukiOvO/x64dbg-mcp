#pragma once
#include <nlohmann/json.hpp>

namespace MCP {

/**
 * @brief Dump操作的 JSON-RPC 处理器
 * 
 * 实现的方法：
 * - dump.module: Dump指定模块到文件
 * - dump.memory_region: Dump指定内存区域
 * - dump.analyze_module: 分析模块是否加壳
 * - dump.detect_oep: 检测原始入口点
 * - dump.get_dumpable_regions: 获取可dump的内存区域
 */
class DumpHandler {
public:
    /**
     * @brief 注册所有dump相关的方法
     */
    static void RegisterMethods();

private:
    /**
     * @brief Dump模块
     * @param params {
     *   "module": "ntdll.dll" | "0x7FF12340000",  // 模块名或基址
     *   "output_path": "C:\\dump\\module.exe",    // 输出文件路径
     *   "options": {                               // 可选
     *     "fix_imports": true,                     // 修复导入表
     *     "fix_relocations": false,                // 修复重定位
     *     "fix_oep": true,                         // 修复入口点
     *     "remove_integrity_check": true,          // 移除校验和
     *     "rebuild_pe": true,                      // 重建PE头
     *     "auto_detect_oep": false,                // 自动检测OEP
     *     "dump_full_image": false                 // dump完整镜像
     *   }
     * }
     * @return {
     *   "success": true,
     *   "file_path": "C:\\dump\\module.exe",
     *   "dumped_size": 1048576,
     *   "original_ep": "0x7FF123410A0",
     *   "new_ep": "0x7FF123410A0"
     * }
     */
    static nlohmann::json DumpModule(const nlohmann::json& params);
    
    /**
     * @brief Dump内存区域
     * @param params {
     *   "address": "0x7FF12340000",
     *   "size": 4096,
     *   "output_path": "C:\\dump\\region.bin",
     *   "as_raw_binary": false  // true=原始二进制, false=尝试修复PE
     * }
     * @return {
     *   "success": true,
     *   "file_path": "C:\\dump\\region.bin",
     *   "dumped_size": 4096
     * }
     */
    static nlohmann::json DumpMemoryRegion(const nlohmann::json& params);
    
    /**
     * @brief 分析模块
     * @param params {
     *   "module": "ntdll.dll" | "0x7FF12340000"
     * }
     * @return {
     *   "name": "ntdll.dll",
     *   "path": "C:\\Windows\\System32\\ntdll.dll",
     *   "base_address": "0x7FF12340000",
     *   "size": 2097152,
     *   "entry_point": "0x7FF123410A0",
     *   "is_packed": false,
     *   "packer_id": ""
     * }
     */
    static nlohmann::json AnalyzeModule(const nlohmann::json& params);
    
    /**
     * @brief 检测OEP
     * @param params {
     *   "module_base": "0x400000"
     * }
     * @return {
     *   "oep": "0x401000",
     *   "rva": "0x1000",
     *   "detected": true
     * }
     */
    static nlohmann::json DetectOEP(const nlohmann::json& params);
    
    /**
     * @brief 获取可dump的内存区域
     * @param params {
     *   "module_base": "0x400000"  // 可选,0表示所有
     * }
     * @return {
     *   "regions": [
     *     {
     *       "address": "0x400000",
     *       "size": 4096,
     *       "protection": "PAGE_EXECUTE_READ",
     *       "type": "MEM_COMMIT",
     *       "name": "module.exe"
     *     }
     *   ]
     * }
     */
    static nlohmann::json GetDumpableRegions(const nlohmann::json& params);
    // 辅助方法
    static nlohmann::json DumpOptionsFromJson(const nlohmann::json& json);
    static nlohmann::json DumpResultToJson(const struct DumpResult& result);
    static nlohmann::json ModuleDumpInfoToJson(const struct ModuleDumpInfo& info);
    static nlohmann::json MemoryRegionDumpToJson(const struct MemoryRegionDump& region);
};

} // namespace MCP
