#include "BreakpointManager.h"
#include "DebugController.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../utils/StringUtils.h"
#include "../core/X64DBGBridge.h"

#ifdef XDBG_SDK_AVAILABLE
#include "_scriptapi_debug.h"
#endif

namespace {
const DBGFUNCTIONS* SafeDbgFunctions() {
    const auto* f = DbgFunctions();
    if (!f) throw MCP::MCPException("x64dbg SDK functions not available");
    return f;
}
} // namespace

namespace MCP {

BreakpointManager& BreakpointManager::Instance() {
    static BreakpointManager instance;
    return instance;
}

bool BreakpointManager::SetSoftwareBreakpoint(uint64_t address, const std::string& name) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    Logger::Debug("Setting software breakpoint at 0x{:X}", address);
    
    // 使用实际SDK的Script API
    if (!Script::Debug::SetBreakpoint(address)) {
        Logger::Error("Failed to set software breakpoint at 0x{:X}", address);
        return false;
    }
    
    if (!name.empty()) {
        RenameBreakpoint(address, name);
    }
    
    Logger::Info("Software breakpoint set at 0x{:X}", address);
    return true;
}

bool BreakpointManager::SetHardwareBreakpoint(
    uint64_t address,
    HardwareBreakpointCondition condition,
    HardwareBreakpointSize size,
    const std::string& name)
{
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    Logger::Debug("Setting hardware breakpoint at 0x{:X}, condition: {}, size: {}", 
                  address, 
                  HardwareConditionToString(condition),
                  static_cast<int>(size));
    
    // 将条件转换为 x64dbg 的格式
    Script::Debug::HardwareType hwType = Script::Debug::HardwareExecute;
    switch (condition) {
        case HardwareBreakpointCondition::Execute:
            hwType = Script::Debug::HardwareExecute;
            break;
        case HardwareBreakpointCondition::Write:
            hwType = Script::Debug::HardwareWrite;
            break;
        case HardwareBreakpointCondition::ReadWrite:
            hwType = Script::Debug::HardwareAccess;
            break;
    }
    
    if (!Script::Debug::SetHardwareBreakpoint(address, hwType)) {
        Logger::Error("Failed to set hardware breakpoint at 0x{:X}", address);
        return false;
    }
    
    if (!name.empty()) {
        RenameBreakpoint(address, name);
    }
    
    Logger::Info("Hardware breakpoint set at 0x{:X}", address);
    return true;
}

bool BreakpointManager::SetMemoryBreakpoint(uint64_t address, size_t size, const std::string& name) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    Logger::Debug("Setting memory breakpoint at 0x{:X}, size: {}", address, size);
    
    // x64dbg SDK doesn't have dedicated memory breakpoint API
    // Use hardware breakpoint with write access as a workaround
    if (!Script::Debug::SetHardwareBreakpoint(address, Script::Debug::HardwareWrite)) {
        Logger::Error("Failed to set memory breakpoint at 0x{:X}", address);
        return false;
    }
    
    if (!name.empty()) {
        RenameBreakpoint(address, name);
    }
    
    Logger::Info("Memory breakpoint set at 0x{:X}", address);
    return true;
}

bool BreakpointManager::DeleteBreakpoint(uint64_t address, std::optional<BreakpointType> type) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    Logger::Debug("Deleting breakpoint at 0x{:X}", address);
    
    // Check with both direct query and full list, because mixed-type breakpoints
    // at the same address can be missed by DbgGetBpxTypeAt in some states.
    int existingType = DbgGetBpxTypeAt(address);

    int listedType = bp_none;
    BPMAP bpMap;
    bpMap.count = 0;
    bpMap.bp = nullptr;

    int listedCount = DbgGetBpList(bp_none, &bpMap);
    if (listedCount > 0 && bpMap.bp != nullptr) {
        for (int i = 0; i < listedCount; i++) {
            const BRIDGEBP& bp = bpMap.bp[i];
            if (bp.addr != address) {
                continue;
            }

            if (bp.type == bp_normal) {
                listedType |= bp_normal;
            } else if (bp.type == bp_hardware) {
                listedType |= bp_hardware;
            } else if (bp.type == bp_memory) {
                listedType |= bp_memory;
            }
        }
    }

    if (bpMap.bp != nullptr) {
        BridgeFree(bpMap.bp);
    }

    existingType |= listedType;

    if (existingType == bp_none) {
        std::string msg = "No breakpoint exists at address: " +
                         StringUtils::FormatAddress(address);
        Logger::Warning(msg);
        throw ResourceNotFoundException(msg);
    }
    
    bool success = false;
    std::string deletedType;
    
    if (!type.has_value()) {
        // 删除所有类型的断点
        bool softDeleted = false;
        bool hardDeleted = false;
        
        if (existingType & bp_normal) {
            softDeleted = Script::Debug::DeleteBreakpoint(address);
            if (softDeleted) deletedType += "software ";
        }
        if (existingType & (bp_hardware | bp_memory)) {
            hardDeleted = Script::Debug::DeleteHardwareBreakpoint(address);
            if (hardDeleted) deletedType += "hardware/memory ";
        }
        
        success = softDeleted || hardDeleted;
    } else {
        switch (type.value()) {
            case BreakpointType::Software:
                if (!(existingType & bp_normal)) {
                    throw ResourceNotFoundException(
                        "No software breakpoint at address: " + 
                        StringUtils::FormatAddress(address)
                    );
                }
                success = Script::Debug::DeleteBreakpoint(address);
                deletedType = "software";
                break;
            case BreakpointType::Hardware:
                if (!(existingType & bp_hardware)) {
                    throw ResourceNotFoundException(
                        "No hardware breakpoint at address: " + 
                        StringUtils::FormatAddress(address)
                    );
                }
                success = Script::Debug::DeleteHardwareBreakpoint(address);
                deletedType = "hardware";
                break;
            case BreakpointType::Memory:
                if (!(existingType & bp_memory)) {
                    throw ResourceNotFoundException(
                        "No memory breakpoint at address: " + 
                        StringUtils::FormatAddress(address)
                    );
                }
                success = Script::Debug::DeleteHardwareBreakpoint(address);
                deletedType = "memory";
                break;
        }
    }
    
    if (success) {
        Logger::Info("Deleted {} breakpoint at 0x{:X}", deletedType, address);
    } else {
        std::string msg = "Failed to delete breakpoint at: " + 
                         StringUtils::FormatAddress(address);
        Logger::Error(msg);
        throw MCPException(msg);
    }
    
    return success;
}

bool BreakpointManager::EnableBreakpoint(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // x64dbg SDK doesn't have EnableBreakpoint API
    // As a workaround, re-set the breakpoint
    if (!Script::Debug::SetBreakpoint(address)) {
        Logger::Error("Failed to enable breakpoint at 0x{:X}", address);
        return false;
    }
    
    Logger::Debug("Breakpoint enabled at 0x{:X}", address);
    return true;
}

bool BreakpointManager::DisableBreakpoint(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    if (!Script::Debug::DisableBreakpoint(address)) {
        Logger::Error("Failed to disable breakpoint at 0x{:X}", address);
        return false;
    }
    
    Logger::Debug("Breakpoint disabled at 0x{:X}", address);
    return true;
}

bool BreakpointManager::ToggleBreakpoint(uint64_t address) {
    auto bp = GetBreakpoint(address);
    if (!bp.has_value()) {
        // 如果断点不存在,创建一个新的软件断点
        return SetSoftwareBreakpoint(address);
    }
    
    if (bp->enabled) {
        return DisableBreakpoint(address);
    } else {
        return EnableBreakpoint(address);
    }
}

std::optional<BreakpointInfo> BreakpointManager::GetBreakpoint(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    int bpType = DbgGetBpxTypeAt(address);
    if (bpType == bp_none) {
        return std::nullopt;
    }
    
    BreakpointInfo info;
    info.address = address;
    info.hitCount = GetHitCount(address);
    info.module = GetModuleName(address);
    
    // 确定断点类型
    BPXTYPE type = bp_none;
    if (bpType & bp_normal) {
        info.type = BreakpointType::Software;
        type = bp_normal;
    } else if (bpType & bp_hardware) {
        info.type = BreakpointType::Hardware;
        info.condition = HardwareBreakpointCondition::Execute;
        info.size = HardwareBreakpointSize::Byte1;
        type = bp_hardware;
    } else if (bpType & bp_memory) {
        info.type = BreakpointType::Memory;
        type = bp_memory;
    }
    
    // 从 x64dbg 获取断点启用状态
    BP_REF bpRef;
    memset(&bpRef, 0, sizeof(bpRef));
    
    if (SafeDbgFunctions()->BpRefVa(&bpRef, type, address)) {
        duint enabled = 0;
        if (SafeDbgFunctions()->BpGetFieldNumber(&bpRef, bpf_enabled, &enabled)) {
            info.enabled = (enabled != 0);
        } else {
            info.enabled = true; // 默认为启用
        }
        
        // 获取日志断点信息
        std::string logText;
        if (SafeDbgFunctions()->BpGetFieldText(&bpRef, bpf_logtext, 
            [](const char* str, void* userdata) {
                if (str) {
                    *(std::string*)userdata = str;
                }
            }, &logText)) {
            info.isLogBreakpoint = !logText.empty();
            info.logMessage = logText;
        } else {
            info.isLogBreakpoint = false;
        }
    } else {
        // 如果无法获取引用，默认为启用
        info.enabled = true;
        info.isLogBreakpoint = false;
    }
    
    return info;
}

std::vector<BreakpointInfo> BreakpointManager::ListBreakpoints(std::optional<BreakpointType> typeFilter) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<BreakpointInfo> breakpoints;
    
    // 使用 x64dbg API 获取断点列表
    BPMAP bpMap;
    bpMap.count = 0;
    bpMap.bp = nullptr;
    
    // 获取所有类型的断点
    int count = DbgGetBpList(bp_none, &bpMap);
    
    if (count > 0 && bpMap.bp != nullptr) {
        for (int i = 0; i < count; i++) {
            const BRIDGEBP& bp = bpMap.bp[i];

            int expectedTypeBit = bp_none;
            if (bp.type == bp_normal) {
                expectedTypeBit = bp_normal;
            } else if (bp.type == bp_hardware) {
                expectedTypeBit = bp_hardware;
            } else if (bp.type == bp_memory) {
                expectedTypeBit = bp_memory;
            } else {
                continue;
            }

            // Filter out stale entries returned by DbgGetBpList.
            // Keep only entries that still exist at address and type.
            int currentType = DbgGetBpxTypeAt(bp.addr);
            if (currentType == bp_none || !(currentType & expectedTypeBit)) {
                continue;
            }

            // 转换为 BreakpointInfo
            BreakpointInfo info;
            info.address = bp.addr;
            info.enabled = bp.enabled;
            info.name = bp.name;

            // 确定断点类型（基于扩展类型字段）
            if (bp.type == bp_normal) {
                info.type = BreakpointType::Software;
            } else if (bp.type == bp_hardware) {
                info.type = BreakpointType::Hardware;

                // 硬件断点详细信息
                switch (bp.typeEx) {
                    case 0: // BPHWEXEC
                        info.condition = HardwareBreakpointCondition::Execute;
                        break;
                    case 1: // BPHWWRITE
                        info.condition = HardwareBreakpointCondition::Write;
                        break;
                    case 3: // BPHWACCESS
                        info.condition = HardwareBreakpointCondition::ReadWrite;
                        break;
                }

                // 硬件断点大小
                switch (bp.hwSize) {
                    case 0: info.size = HardwareBreakpointSize::Byte1; break;
                    case 1: info.size = HardwareBreakpointSize::Byte2; break;
                    case 2: info.size = HardwareBreakpointSize::Byte4; break;
                    case 3: info.size = HardwareBreakpointSize::Byte8; break;
                }
            } else if (bp.type == bp_memory) {
                info.type = BreakpointType::Memory;

                // 内存断点详细信息（使用 typeEx 判断访问类型）
                switch (bp.typeEx) {
                    case 0: // Read
                        info.condition = HardwareBreakpointCondition::ReadWrite;
                        break;
                    case 1: // Write
                        info.condition = HardwareBreakpointCondition::Write;
                        break;
                    case 2: // Execute
                        info.condition = HardwareBreakpointCondition::Execute;
                        break;
                }
            }
            
            // 应用类型过滤器
            if (typeFilter.has_value() && info.type != typeFilter.value()) {
                continue;
            }
            
            // 填充其他信息
            info.hitCount = bp.hitCount;
            info.module = bp.mod;
            info.condition_expr = bp.breakCondition;
            info.isLogBreakpoint = !std::string(bp.logText).empty();
            info.logMessage = bp.logText;
            
            breakpoints.push_back(info);
        }
        
        // 释放内存
        if (bpMap.bp) {
            BridgeFree(bpMap.bp);
        }
    }
    
    Logger::Debug("Listed {} breakpoints", breakpoints.size());
    return breakpoints;
}

size_t BreakpointManager::DeleteAllBreakpoints(std::optional<BreakpointType> typeFilter) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    auto breakpoints = ListBreakpoints(typeFilter);
    size_t deletedCount = 0;
    size_t skippedCount = 0;
    
    for (const auto& bp : breakpoints) {
        try {
            if (DeleteBreakpoint(bp.address, bp.type)) {
                deletedCount++;
            }
        } catch (const ResourceNotFoundException&) {
            // Breakpoint list can contain stale entries when a previous
            // session was closed. Skip these entries and continue cleanup.
            skippedCount++;
            Logger::Warning("Skip stale breakpoint while deleting all: 0x{:X}", bp.address);
        } catch (const std::exception& ex) {
            skippedCount++;
            Logger::Warning("Failed to delete breakpoint 0x{:X}: {}", bp.address, ex.what());
        }
    }
    
    if (skippedCount > 0) {
        Logger::Info("Deleted {} breakpoints ({} skipped)", deletedCount, skippedCount);
    } else {
        Logger::Info("Deleted {} breakpoints", deletedCount);
    }
    return deletedCount;
}

bool BreakpointManager::SetCondition(uint64_t address, const std::string& condition) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 首先检查断点是否存在
    if (!HasBreakpoint(address)) {
        Logger::Warning("No breakpoint exists at 0x{:X}", address);
        return false;
    }
    
    // 使用 x64dbg 的 DbgFunctions API 设置断点条件
    // 先创建断点引用
    BP_REF bpRef;
    memset(&bpRef, 0, sizeof(bpRef));
    
    // 根据地址创建断点引用（使用虚拟地址）
    // 需要确定断点类型
    int bpType = DbgGetBpxTypeAt(address);
    BPXTYPE type = bp_none;
    if (bpType & bp_normal) {
        type = bp_normal;
    } else if (bpType & bp_hardware) {
        type = bp_hardware;
    } else if (bpType & bp_memory) {
        type = bp_memory;
    }
    
    if (!SafeDbgFunctions()->BpRefVa(&bpRef, type, address)) {
        Logger::Error("Failed to get breakpoint reference at 0x{:X}", address);
        return false;
    }
    
    // 设置断点条件
    if (!SafeDbgFunctions()->BpSetFieldText(&bpRef, bpf_breakcondition, condition.c_str())) {
        Logger::Error("Failed to set condition for breakpoint at 0x{:X}", address);
        return false;
    }
    
    Logger::Info("Set condition for breakpoint at 0x{:X}: {}", address, condition);
    return true;
}

bool BreakpointManager::SetLogBreakpoint(uint64_t address, const std::string& message) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 首先检查断点是否存在
    if (!HasBreakpoint(address)) {
        Logger::Warning("No breakpoint exists at 0x{:X}", address);
        return false;
    }
    
    // 使用 x64dbg 的 DbgFunctions API 设置日志消息
    BP_REF bpRef;
    memset(&bpRef, 0, sizeof(bpRef));
    
    int bpType = DbgGetBpxTypeAt(address);
    BPXTYPE type = bp_none;
    if (bpType & bp_normal) {
        type = bp_normal;
    } else if (bpType & bp_hardware) {
        type = bp_hardware;
    } else if (bpType & bp_memory) {
        type = bp_memory;
    }
    
    if (!SafeDbgFunctions()->BpRefVa(&bpRef, type, address)) {
        Logger::Error("Failed to get breakpoint reference at 0x{:X}", address);
        return false;
    }
    
    // 设置日志消息
    if (!SafeDbgFunctions()->BpSetFieldText(&bpRef, bpf_logtext, message.c_str())) {
        Logger::Error("Failed to set log message for breakpoint at 0x{:X}", address);
        return false;
    }
    
    Logger::Info("Set log breakpoint at 0x{:X}: {}", address, message);
    return true;
}

bool BreakpointManager::RenameBreakpoint(uint64_t address, const std::string& name) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 首先检查断点是否存在
    if (!HasBreakpoint(address)) {
        Logger::Warning("No breakpoint exists at 0x{:X}", address);
        return false;
    }
    
    // 使用 x64dbg 的 DbgFunctions API 重命名断点
    BP_REF bpRef;
    memset(&bpRef, 0, sizeof(bpRef));
    
    int bpType = DbgGetBpxTypeAt(address);
    BPXTYPE type = bp_none;
    if (bpType & bp_normal) {
        type = bp_normal;
    } else if (bpType & bp_hardware) {
        type = bp_hardware;
    } else if (bpType & bp_memory) {
        type = bp_memory;
    }
    
    if (!SafeDbgFunctions()->BpRefVa(&bpRef, type, address)) {
        Logger::Error("Failed to get breakpoint reference at 0x{:X}", address);
        return false;
    }
    
    // 设置断点名称
    if (!SafeDbgFunctions()->BpSetFieldText(&bpRef, bpf_name, name.c_str())) {
        Logger::Error("Failed to rename breakpoint at 0x{:X}", address);
        return false;
    }
    
    Logger::Info("Renamed breakpoint at 0x{:X} to '{}'", address, name);
    return true;
}

bool BreakpointManager::HasBreakpoint(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        return false;
    }
    
    return DbgGetBpxTypeAt(address) != bp_none;
}

uint32_t BreakpointManager::GetHitCount(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        return 0;
    }
    
    // 检查断点是否存在
    int bpType = DbgGetBpxTypeAt(address);
    if (bpType == bp_none) {
        return 0;
    }
    
    // 使用 DbgFunctions API 获取命中次数
    BP_REF bpRef;
    memset(&bpRef, 0, sizeof(bpRef));
    
    BPXTYPE type = bp_none;
    if (bpType & bp_normal) {
        type = bp_normal;
    } else if (bpType & bp_hardware) {
        type = bp_hardware;
    } else if (bpType & bp_memory) {
        type = bp_memory;
    }
    
    if (!SafeDbgFunctions()->BpRefVa(&bpRef, type, address)) {
        Logger::Warning("Failed to get breakpoint reference at 0x{:X}", address);
        return 0;
    }
    
    // 获取命中次数字段
    duint hitCount = 0;
    if (SafeDbgFunctions()->BpGetFieldNumber(&bpRef, bpf_hitcount, &hitCount)) {
        return static_cast<uint32_t>(hitCount);
    }
    
    return 0;
}

bool BreakpointManager::ResetHitCount(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 检查断点是否存在
    if (!HasBreakpoint(address)) {
        Logger::Warning("No breakpoint exists at 0x{:X}", address);
        return false;
    }
    
    // 使用 DbgFunctions API 重置命中次数
    BP_REF bpRef;
    memset(&bpRef, 0, sizeof(bpRef));
    
    int bpType = DbgGetBpxTypeAt(address);
    BPXTYPE type = bp_none;
    if (bpType & bp_normal) {
        type = bp_normal;
    } else if (bpType & bp_hardware) {
        type = bp_hardware;
    } else if (bpType & bp_memory) {
        type = bp_memory;
    }
    
    if (!SafeDbgFunctions()->BpRefVa(&bpRef, type, address)) {
        Logger::Error("Failed to get breakpoint reference at 0x{:X}", address);
        return false;
    }
    
    // 重置命中次数为 0
    if (!SafeDbgFunctions()->BpSetFieldNumber(&bpRef, bpf_hitcount, 0)) {
        Logger::Error("Failed to reset hit count for breakpoint at 0x{:X}", address);
        return false;
    }
    
    Logger::Info("Reset hit count for breakpoint at 0x{:X}", address);
    return true;
}

std::string BreakpointManager::BreakpointTypeToString(BreakpointType type) {
    switch (type) {
        case BreakpointType::Software:
            return "Software";
        case BreakpointType::Hardware:
            return "Hardware";
        case BreakpointType::Memory:
            return "Memory";
        default:
            return "Unknown";
    }
}

std::string BreakpointManager::HardwareConditionToString(HardwareBreakpointCondition condition) {
    switch (condition) {
        case HardwareBreakpointCondition::Execute:
            return "Execute";
        case HardwareBreakpointCondition::Write:
            return "Write";
        case HardwareBreakpointCondition::ReadWrite:
            return "ReadWrite";
        default:
            return "Unknown";
    }
}

std::string BreakpointManager::GetModuleName(uint64_t address) {
    // 使用 x64dbg API 获取模块名
    // DbgGetModuleAt(address, modName);
    
    return "";  // 占位
}

} // namespace MCP
