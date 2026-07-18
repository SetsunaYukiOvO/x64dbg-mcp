#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

using json = nlohmann::json;

namespace MCP {

/**
 * @brief 请求验证器
 */
class RequestValidator {
public:
    /**
     * @brief 验证参数是否包含必需字段
     * @param params 参数对象
     * @param fieldName 字段名
     * @throws InvalidParamsException 字段不存在
     */
    static void RequireField(const json& params, const std::string& fieldName);
    
    /**
     * @brief 验证字段类型为字符串
     * @param params 参数对象
     * @param fieldName 字段名
     * @throws InvalidParamsException 字段类型错误
     */
    static void RequireString(const json& params, const std::string& fieldName);
    
    /**
     * @brief 验证字段类型为数字
     * @param params 参数对象
     * @param fieldName 字段名
     * @throws InvalidParamsException 字段类型错误
     */
    static void RequireNumber(const json& params, const std::string& fieldName);
    
    /**
     * @brief 验证字段类型为整数
     * @param params 参数对象
     * @param fieldName 字段名
     * @throws InvalidParamsException 字段类型错误
     */
    static void RequireInteger(const json& params, const std::string& fieldName);
    
    /**
     * @brief 验证字段类型为布尔
     * @param params 参数对象
     * @param fieldName 字段名
     * @throws InvalidParamsException 字段类型错误
     */
    static void RequireBoolean(const json& params, const std::string& fieldName);
    
    /**
     * @brief 验证字段类型为对象
     * @param params 参数对象
     * @param fieldName 字段名
     * @throws InvalidParamsException 字段类型错误
     */
    static void RequireObject(const json& params, const std::string& fieldName);
    
    /**
     * @brief 验证字段类型为数组
     * @param params 参数对象
     * @param fieldName 字段名
     * @throws InvalidParamsException 字段类型错误
     */
    static void RequireArray(const json& params, const std::string& fieldName);
    
    /**
     * @brief 获取字符串字段（带默认值）
     * @param params 参数对象
     * @param fieldName 字段名
     * @param defaultValue 默认值
     * @return 字段值
     */
    static std::string GetString(const json& params, 
                                 const std::string& fieldName, 
                                 const std::string& defaultValue = "");
    
    /**
     * @brief 获取整数字段（带默认值）
     * @param params 参数对象
     * @param fieldName 字段名
     * @param defaultValue 默认值
     * @return 字段值
     */
    static int64_t GetInteger(const json& params, 
                             const std::string& fieldName, 
                             int64_t defaultValue = 0);
    
    /**
     * @brief 获取布尔字段（带默认值）
     * @param params 参数对象
     * @param fieldName 字段名
     * @param defaultValue 默认值
     * @return 字段值
     */
    static bool GetBoolean(const json& params, 
                          const std::string& fieldName, 
                          bool defaultValue = false);
    
    /**
     * @brief 验证地址字符串格式
     * @param address 地址字符串
     * @return 解析后的地址
     * @throws InvalidAddressException 地址格式错误
     */
    static uint64_t ValidateAddress(const std::string& address);

    /**
     * @brief 验证字符串或无符号整数地址
     * @param address 地址值
     * @return 解析后的地址
     * @throws InvalidAddressException 地址格式错误或超出目标架构范围
     */
    static uint64_t ValidateAddress(const json& address);

    /**
     * @brief 验证地址字段并返回其值
     * @param params 参数对象
     * @param fieldName 字段名
     * @return 解析后的地址
     * @throws InvalidParamsException 字段不存在
     * @throws InvalidAddressException 地址类型、格式或范围错误
     */
    static uint64_t GetAddress(const json& params, const std::string& fieldName);

    /**
     * @brief 尝试把地址或名称解析为目标架构地址
     * @param addressOrName 地址、表达式或名称
     * @return 可解析地址；名称返回空
     * @throws InvalidAddressException 可解析值超出目标架构范围
     */
    static std::optional<uint64_t> TryValidateAddressOrName(
        const std::string& addressOrName);

    /**
     * @brief 验证大小参数
     * @param size 大小值
     * @param maxSize 最大允许大小
     * @throws InvalidSizeException 大小无效
     */
    static void ValidateSize(size_t size, size_t maxSize);
};

} // namespace MCP
