#include "BreakpointManager.h"
#include "DebugController.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../utils/StringUtils.h"
#include "../core/X64DBGBridge.h"
#include <limits>
#include <sstream>
#include <memory>
#include <algorithm>

#ifdef XDBG_SDK_AVAILABLE
#include "_scriptapi_debug.h"
#endif

namespace {
const DBGFUNCTIONS* SafeDbgFunctions() {
    const auto* f = DbgFunctions();
    if (!f) throw MCP::MCPException("x64dbg SDK functions not available");
    return f;
}

std::vector<BRIDGEBP> BreakpointSnapshot() {
    BPMAP map{};
    const int count = DbgGetBpList(bp_none, &map);
    const std::unique_ptr<BRIDGEBP, decltype(&BridgeFree)> storage(map.bp, &BridgeFree);
    if (count <= 0 || map.count <= 0 || !map.bp) return {};
    return {map.bp, map.bp + std::min(count, map.count)};
}

int SnapshotTypes(uint64_t address) {
    int types = bp_none;
    for (const auto& bp : BreakpointSnapshot()) {
        if (bp.addr == address && (bp.type == bp_normal || bp.type == bp_hardware || bp.type == bp_memory))
            types |= bp.type;
    }
    return types;
}

BPXTYPE PreferredType(uint64_t address) {
    const int types = SnapshotTypes(address);
    if (types & bp_normal) return bp_normal;
    if (types & bp_hardware) return bp_hardware;
    if (types & bp_memory) return bp_memory;
    return bp_none;
}

const char* HardwareConditionCommand(MCP::HardwareBreakpointCondition condition) {
    switch (condition) {
        case MCP::HardwareBreakpointCondition::Execute:
            return "x";
        case MCP::HardwareBreakpointCondition::Write:
            return "w";
        case MCP::HardwareBreakpointCondition::ReadWrite:
            return "r";
    }
    throw MCP::InvalidParamsException("Invalid hardware breakpoint condition");
}

bool DeleteMemoryBreakpointAt(uint64_t address) {
    std::ostringstream command;
    command << "bpmc 0x" << std::hex << std::uppercase << address;
    return DbgCmdExecDirect(command.str().c_str());
}

BPHWTYPE ToSdkHardwareType(MCP::HardwareBreakpointCondition condition) {
    switch (condition) {
        case MCP::HardwareBreakpointCondition::Execute:
            return hw_execute;
        case MCP::HardwareBreakpointCondition::Write:
            return hw_write;
        case MCP::HardwareBreakpointCondition::ReadWrite:
            return hw_access;
    }
    throw MCP::InvalidParamsException("Invalid hardware breakpoint condition");
}

BPHWSIZE ToSdkHardwareSize(MCP::HardwareBreakpointSize size) {
    switch (size) {
        case MCP::HardwareBreakpointSize::Byte1:
            return hw_byte;
        case MCP::HardwareBreakpointSize::Byte2:
            return hw_word;
        case MCP::HardwareBreakpointSize::Byte4:
            return hw_dword;
        case MCP::HardwareBreakpointSize::Byte8:
            return hw_qword;
    }
    throw MCP::InvalidParamsException("Invalid hardware breakpoint size");
}

bool HardwareBreakpointMatches(
    duint address,
    MCP::HardwareBreakpointCondition condition,
    MCP::HardwareBreakpointSize size)
{
    BP_REF reference{};
    if (!SafeDbgFunctions()->BpRefVa(&reference, bp_hardware, address)) {
        return false;
    }

    duint actualType = 0;
    duint actualSize = 0;
    return SafeDbgFunctions()->BpGetFieldNumber(&reference, bpf_typeex, &actualType) &&
           SafeDbgFunctions()->BpGetFieldNumber(&reference, bpf_hwsize, &actualSize) &&
           actualType == static_cast<duint>(ToSdkHardwareType(condition)) &&
           actualSize == static_cast<duint>(ToSdkHardwareSize(size));
}

bool MemoryBreakpointMatches(duint address, size_t size) {
    return (SnapshotTypes(address) & bp_memory) != 0 &&
           SafeDbgFunctions()->MemBpSize &&
           SafeDbgFunctions()->MemBpSize(address) == static_cast<duint>(size);
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
    
    const auto sizeBytes = static_cast<uint64_t>(size);
    if (!IsHardwareBreakpointSizeSupported(size)) {
        throw InvalidParamsException("Unsupported hardware breakpoint size for this architecture");
    }
    if (!IsHardwareBreakpointRequestValid(
            address,
            condition,
            size,
            std::numeric_limits<duint>::max())) {
        if (address > std::numeric_limits<duint>::max()) {
            throw InvalidParamsException("Hardware breakpoint address exceeds the target address space");
        }
        if (condition == HardwareBreakpointCondition::Execute) {
            throw InvalidParamsException("Execute hardware breakpoints must use a size of 1 byte");
        }
        throw InvalidParamsException("Hardware breakpoint address must be aligned to its size");
    }
    const duint targetAddress = static_cast<duint>(address);

    Logger::Debug("Setting hardware breakpoint at 0x{:X}, condition: {}, size: {}",
                  address,
                  HardwareConditionToString(condition),
                  static_cast<int>(size));

    std::ostringstream command;
    command << "bphws 0x" << std::hex << std::uppercase << address
            << ", " << HardwareConditionCommand(condition)
            << ", " << std::dec << sizeBytes;
    const bool commandSucceeded = DbgCmdExecDirect(command.str().c_str());
    if (!commandSucceeded) {
        Logger::Error("Failed to set hardware breakpoint at 0x{:X}", address);
        return false;
    }
    if (!HardwareBreakpointMatches(targetAddress, condition, size)) {
        Script::Debug::DeleteHardwareBreakpoint(targetAddress);
        Logger::Error("Hardware breakpoint at 0x{:X} did not match the requested condition and size", address);
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
    
    if (size == 0 || size > std::numeric_limits<duint>::max()) {
        throw InvalidParamsException("Memory breakpoint size must fit the target architecture and be greater than zero");
    }
    if (address > std::numeric_limits<duint>::max()) {
        throw InvalidParamsException("Memory breakpoint address exceeds the target address space");
    }
    if (address > std::numeric_limits<duint>::max() - (size - 1)) {
        throw InvalidParamsException("Memory breakpoint range exceeds the target address space");
    }
    const duint targetAddress = static_cast<duint>(address);

    Logger::Debug("Setting memory breakpoint at 0x{:X}, size: {}", address, size);

    std::ostringstream command;
    command << "bpmrange 0x" << std::hex << std::uppercase << address
            << ", 0x" << size << ", a";
    const bool commandSucceeded = DbgCmdExecDirect(command.str().c_str());
    if (!commandSucceeded) {
        Logger::Error("Failed to set memory breakpoint at 0x{:X}", address);
        return false;
    }
    if (!MemoryBreakpointMatches(targetAddress, size)) {
        DeleteMemoryBreakpointAt(targetAddress);
        Logger::Error("Memory breakpoint at 0x{:X} did not match the requested size", address);
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
    
    const int existingType = SnapshotTypes(address);

    if (existingType == bp_none) {
        std::string msg = "No breakpoint exists at address: " +
                         StringUtils::FormatAddress(address);
        Logger::Warning(msg);
        throw ResourceNotFoundException(msg);
    }
    
    bool success = false;
    std::string deletedType;
    
    if (!type.has_value()) {
        success = true;
        if (existingType & bp_normal) success = Script::Debug::DeleteBreakpoint(address) && success;
        if (existingType & bp_hardware) success = Script::Debug::DeleteHardwareBreakpoint(address) && success;
        if (existingType & bp_memory) success = DeleteMemoryBreakpointAt(address) && success;
        deletedType = "all";
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
                success = DeleteMemoryBreakpointAt(address);
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

bool BreakpointManager::EnableBreakpoint(uint64_t address, std::optional<BreakpointType> type) {
    const auto bp = GetBreakpoint(address, type);
    if (!bp) throw ResourceNotFoundException("No matching breakpoint at address");
    const char* verb = bp->type == BreakpointType::Software ? "bpe" :
                       bp->type == BreakpointType::Hardware ? "bphe" : "bpme";
    const std::string command = std::string(verb) + " " + StringUtils::FormatAddress(address);
    if (!DbgCmdExecDirect(command.c_str())) return false;
    const auto updated = GetBreakpoint(address, bp->type);
    return updated && updated->enabled;
}

bool BreakpointManager::DisableBreakpoint(uint64_t address, std::optional<BreakpointType> type) {
    const auto bp = GetBreakpoint(address, type);
    if (!bp) throw ResourceNotFoundException("No matching breakpoint at address");
    const char* verb = bp->type == BreakpointType::Software ? "bpd" :
                       bp->type == BreakpointType::Hardware ? "bphd" : "bpmd";
    const std::string command = std::string(verb) + " " + StringUtils::FormatAddress(address);
    if (!DbgCmdExecDirect(command.c_str())) return false;
    const auto updated = GetBreakpoint(address, bp->type);
    return updated && !updated->enabled;
}

bool BreakpointManager::ToggleBreakpoint(uint64_t address, std::optional<BreakpointType> type) {
    const auto bp = GetBreakpoint(address, type);
    if (!bp) {
        if (type && *type != BreakpointType::Software)
            throw ResourceNotFoundException("No matching breakpoint at address");
        return SetSoftwareBreakpoint(address);
    }
    return bp->enabled ? DisableBreakpoint(address, bp->type) : EnableBreakpoint(address, bp->type);
}

std::optional<BreakpointInfo> BreakpointManager::GetBreakpoint(uint64_t address, std::optional<BreakpointType> typeFilter) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    const auto breakpoints = ListBreakpoints();
    // Deterministic priority for same-address queries: software, hardware, memory.
    for (const auto type : {BreakpointType::Software, BreakpointType::Hardware, BreakpointType::Memory})
        for (const auto& bp : breakpoints)
            if (bp.address == address && bp.type == type && (!typeFilter || type == *typeFilter)) return bp;
    return std::nullopt;
}

std::vector<BreakpointInfo> BreakpointManager::ListBreakpoints(std::optional<BreakpointType> typeFilter) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<BreakpointInfo> breakpoints;
    
    for (const BRIDGEBP& bp : BreakpointSnapshot()) {
            if (bp.type != bp_normal && bp.type != bp_hardware && bp.type != bp_memory) continue;
            // 转换为 BreakpointInfo
            BreakpointInfo info{};
            info.address = bp.addr;
            info.enabled = bp.enabled;
            info.name = bp.name;

            // 确定断点类型（基于扩展类型字段）
            if (bp.type == bp_normal) {
                info.type = BreakpointType::Software;
            } else if (bp.type == bp_hardware) {
                info.type = BreakpointType::Hardware;

                switch (static_cast<BPHWTYPE>(bp.typeEx)) {
                    case hw_access:
                        info.condition = HardwareBreakpointCondition::ReadWrite;
                        break;
                    case hw_write:
                        info.condition = HardwareBreakpointCondition::Write;
                        break;
                    case hw_execute:
                        info.condition = HardwareBreakpointCondition::Execute;
                        break;
                }

                switch (static_cast<BPHWSIZE>(bp.hwSize)) {
                    case hw_byte: info.size = HardwareBreakpointSize::Byte1; break;
                    case hw_word: info.size = HardwareBreakpointSize::Byte2; break;
                    case hw_dword: info.size = HardwareBreakpointSize::Byte4; break;
                    case hw_qword: info.size = HardwareBreakpointSize::Byte8; break;
                }
            } else if (bp.type == bp_memory) {
                info.type = BreakpointType::Memory;

                switch (static_cast<BPMEMTYPE>(bp.typeEx)) {
                    case mem_access:
                    case mem_read:
                        info.condition = HardwareBreakpointCondition::ReadWrite;
                        break;
                    case mem_write:
                        info.condition = HardwareBreakpointCondition::Write;
                        break;
                    case mem_execute:
                        info.condition = HardwareBreakpointCondition::Execute;
                        break;
                }
                if (SafeDbgFunctions()->MemBpSize) {
                    info.memorySize = static_cast<size_t>(SafeDbgFunctions()->MemBpSize(bp.addr));
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
    const BPXTYPE type = PreferredType(address);
    
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
    
    const BPXTYPE type = PreferredType(address);
    
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
    
    const BPXTYPE type = PreferredType(address);
    
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
    
    return PreferredType(address) != bp_none;
}

uint32_t BreakpointManager::GetHitCount(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        return 0;
    }
    
    const auto bp = GetBreakpoint(address);
    return bp ? bp->hitCount : 0;
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
    
    const BPXTYPE type = PreferredType(address);
    
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
