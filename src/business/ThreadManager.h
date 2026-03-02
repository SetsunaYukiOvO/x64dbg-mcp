#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace MCP {

/**
 * @brief 线程信息
 */
struct ThreadInfo {
    uint32_t id;                // 线程ID
    uint64_t handle;            // 线程句柄
    uint64_t entry;             // 入口点地址
    uint64_t teb;               // 线程环境块（TEB）地址
    std::string name;           // 线程名称（如果有）
    bool isCurrent;             // 是否当前线程
    bool isSuspended;           // 是否挂起
    int priority;               // 优先级
    uint64_t rip;               // 当前指令指针
    
    // 上下文信息
    uint64_t rsp;               // 栈指针
    uint64_t rbp;               // 基址指针
};

/**
 * @brief 线程管理器
 * 封装 x64dbg 线程管理 API
 */
class ThreadManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ThreadManager& Instance();
    
    /**
     * @brief 获取所有线程列表
     * @return 线程信息列表
     */
    std::vector<ThreadInfo> GetThreadList();
    
    /**
     * @brief 获取当前线程ID
     * @return 当前线程ID
     */
    uint32_t GetCurrentThreadId();
    
    /**
     * @brief 获取当前线程信息
     * @return 当前线程详细信息
     */
    ThreadInfo GetCurrentThread();
    
    /**
     * @brief 获取指定线程信息
     * @param threadId 线程ID
     * @return 线程信息
     */
    ThreadInfo GetThread(uint32_t threadId);
    
    /**
     * @brief 切换到指定线程
     * @param threadId 目标线程ID
     * @return 是否成功
     */
    bool SwitchThread(uint32_t threadId);
    
    /**
     * @brief 挂起指定线程
     * @param threadId 线程ID
     * @return 是否成功
     */
    bool SuspendThread(uint32_t threadId);
    
    /**
     * @brief 恢复指定线程
     * @param threadId 线程ID
     * @return 是否成功
     */
    bool ResumeThread(uint32_t threadId);
    
    /**
     * @brief 获取线程数量
     * @return 线程总数
     */
    size_t GetThreadCount();
    
    /**
     * @brief 检查线程ID是否有效
     * @param threadId 线程ID
     * @return 是否有效
     */
    bool IsValidThread(uint32_t threadId);
    
private:
    ThreadManager() = default;
    ~ThreadManager() = default;
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;
    
    /**
     * @brief 获取线程名称
     * @param threadId 线程ID
     * @return 线程名称（如果有）
     */
    std::string GetThreadName(uint32_t threadId);
};

} // namespace MCP
