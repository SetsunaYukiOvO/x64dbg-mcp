#include "DisassemblyEngine.h"
#include "DebugController.h"
#include "MemoryManager.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../core/TargetValueValidator.h"
#include "../utils/StringUtils.h"
#include "../core/X64DBGBridge.h"
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace MCP {

DisassemblyEngine& DisassemblyEngine::Instance() {
    static DisassemblyEngine instance;
    return instance;
}

InstructionInfo DisassemblyEngine::DisassembleAt(uint64_t address) {
    if (!TargetValueValidator::FitsAddress(address)) {
        throw InvalidAddressException("Address exceeds target architecture range: " +
                                      StringUtils::FormatAddress(address));
    }

    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 首先检查内存是否可读
    if (!DbgMemIsValidReadPtr(address)) {
        throw MCPException(
            "Memory not readable at address: " + 
            StringUtils::FormatAddress(address)
        );
    }
    
    BASIC_INSTRUCTION_INFO basic = {};
    DbgDisasmFastAt(address, &basic);
    
    // 验证反汇编是否成功（size > 0 表示成功）
    if (basic.size == 0) {
        throw MCPException(
            "Failed to disassemble at address: " + 
            StringUtils::FormatAddress(address)
        );
    }
    
    InstructionInfo info;
    info.address = address;
    info.size = basic.size;
    info.full = basic.instruction;
    
    // 读取字节码
    unsigned char buffer[16] = {0};
    if (basic.size > 0 && basic.size <= sizeof(buffer)) {
        if (DbgMemRead(address, buffer, basic.size)) {
            info.bytes.assign(buffer, buffer + basic.size);
        }
    }
    
    // 解析助记符和操作数
    std::string fullInstr = basic.instruction;
    size_t spacePos = fullInstr.find(' ');
    if (spacePos != std::string::npos) {
        info.mnemonic = fullInstr.substr(0, spacePos);
        info.operands = fullInstr.substr(spacePos + 1);
    } else {
        info.mnemonic = fullInstr;
        info.operands = "";
    }
    
    // 分析指令特性
    info.isBranch = basic.branch;
    info.isCall = basic.call;
    // 检查是否是 ret 指令
    info.isRet = (info.mnemonic == "ret" || info.mnemonic == "retn" || 
                  info.mnemonic == "retf" || info.mnemonic == "iret");
    
    if (basic.branch || basic.call) {
        info.branchTarget = basic.addr;
    }
    
    Logger::Trace("Disassembled at 0x{:X}: {}", address, info.full);
    return info;
}

std::vector<InstructionInfo> DisassemblyEngine::DisassembleRange(uint64_t address, size_t count) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<InstructionInfo> instructions;
    instructions.reserve(count);
    
    uint64_t currentAddr = address;
    
    for (size_t i = 0; i < count; ++i) {
        try {
            auto instr = DisassembleAt(currentAddr);
            instructions.push_back(instr);
            currentAddr += instr.size;
        } catch (const MCPException& e) {
            Logger::Warning("Failed to disassemble at 0x{:X}: {}", currentAddr, e.what());
            break;
        }
    }
    
    Logger::Debug("Disassembled {} instructions from 0x{:X}", instructions.size(), address);
    return instructions;
}

std::vector<InstructionInfo> DisassemblyEngine::DisassembleBytes(uint64_t address, size_t size) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<InstructionInfo> instructions;
    uint64_t currentAddr = address;
    uint64_t endAddr = address + size;
    
    while (currentAddr < endAddr) {
        try {
            auto instr = DisassembleAt(currentAddr);
            instructions.push_back(instr);
            currentAddr += instr.size;
        } catch (const MCPException& e) {
            Logger::Warning("Failed to disassemble at 0x{:X}: {}", currentAddr, e.what());
            break;
        }
    }
    
    Logger::Debug("Disassembled {} bytes into {} instructions from 0x{:X}", 
                  size, instructions.size(), address);
    return instructions;
}

std::vector<InstructionInfo> DisassemblyEngine::DisassembleFunction(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<InstructionInfo> instructions;
    uint64_t currentAddr = address;
    
    // 简单的函数边界检测: 遇到 ret 指令停止
    // 更复杂的实现需要考虑多个返回点、跳转表等
    const size_t maxInstructions = 10000; // 防止无限循环
    
    for (size_t i = 0; i < maxInstructions; ++i) {
        try {
            auto instr = DisassembleAt(currentAddr);
            instructions.push_back(instr);
            
            // 遇到返回指令停止
            if (instr.isRet) {
                break;
            }
            
            currentAddr += instr.size;
        } catch (const MCPException& e) {
            Logger::Warning("Failed to disassemble function at 0x{:X}: {}", currentAddr, e.what());
            break;
        }
    }
    
    Logger::Debug("Disassembled function at 0x{:X}: {} instructions", 
                  address, instructions.size());
    return instructions;
}

size_t DisassemblyEngine::GetInstructionLength(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // Use DbgDisasmFastAt to get instruction size
    BASIC_INSTRUCTION_INFO basic = {};
    DbgDisasmFastAt(address, &basic);
    
    if (basic.size == 0) {
        char addrStr[32];
        snprintf(addrStr, sizeof(addrStr), "0x%llX", (unsigned long long)address);
        throw InvalidAddressException(std::string("Invalid instruction at: ") + addrStr);
    }
    
    return basic.size;
}

uint64_t DisassemblyEngine::GetNextInstruction(uint64_t address) {
    size_t length = GetInstructionLength(address);
    return address + length;
}

uint64_t DisassemblyEngine::GetPreviousInstruction(uint64_t address) {
    // 向前搜索上一条指令
    // x86/x64 是变长指令集,需要从前面开始反汇编
    // 这里使用简单的向后搜索策略
    
    const size_t maxSearchBack = 15; // x64 最长指令15字节
    
    for (size_t offset = 1; offset <= maxSearchBack; ++offset) {
        try {
            uint64_t testAddr = address - offset;
            size_t length = GetInstructionLength(testAddr);
            
            if (testAddr + length == address) {
                return testAddr;
            }
        } catch (...) {
            continue;
        }
    }
    
    throw MCPException("Cannot find previous instruction before: " + 
                      StringUtils::FormatAddress(address));
}

bool DisassemblyEngine::IsValidInstruction(uint64_t address) {
    try {
        GetInstructionLength(address);
        return true;
    } catch (...) {
        return false;
    }
}

std::string DisassemblyEngine::FormatInstruction(const InstructionInfo& info,
                                                bool includeBytes,
                                                bool includeAddress)
{
    std::ostringstream oss;
    
    if (includeAddress) {
        char addrStr[32];
        snprintf(addrStr, sizeof(addrStr), "%016llX", (unsigned long long)info.address);
        oss << addrStr << "  ";
    }
    
    if (includeBytes) {
        // 格式化字节码 (最多显示8字节)
        size_t bytesToShow = std::min(info.bytes.size(), size_t(8));
        for (size_t i = 0; i < bytesToShow; ++i) {
            char byteStr[4];
            snprintf(byteStr, sizeof(byteStr), "%02X", info.bytes[i]);
            oss << byteStr;
        }
        
        // 补齐空格 (对齐)
        for (size_t i = bytesToShow; i < 8; ++i) {
            oss << "  ";
        }
        
        oss << "  ";
    }
    
    oss << info.full;
    
    if (!info.comment.empty()) {
        oss << "  ; " << info.comment;
    }
    
    return oss.str();
}

void DisassemblyEngine::AnalyzeInstruction(InstructionInfo& info) {
    info.isBranch = IsBranchInstruction(info.mnemonic);
    info.isCall = IsCallInstruction(info.mnemonic);
    info.isRet = IsRetInstruction(info.mnemonic);
    info.branchTarget = ExtractBranchTarget(info);
}

bool DisassemblyEngine::IsBranchInstruction(const std::string& mnemonic) {
    // 转换为小写
    std::string lower = StringUtils::ToLower(mnemonic);
    
    // 检查是否为分支指令
    return lower.find("jmp") == 0 ||
           lower.find("je") == 0 ||
           lower.find("jne") == 0 ||
           lower.find("jz") == 0 ||
           lower.find("jnz") == 0 ||
           lower.find("ja") == 0 ||
           lower.find("jb") == 0 ||
           lower.find("jg") == 0 ||
           lower.find("jl") == 0 ||
           lower.find("jae") == 0 ||
           lower.find("jbe") == 0 ||
           lower.find("jge") == 0 ||
           lower.find("jle") == 0 ||
           lower.find("jo") == 0 ||
           lower.find("jno") == 0 ||
           lower.find("js") == 0 ||
           lower.find("jns") == 0;
}

bool DisassemblyEngine::IsCallInstruction(const std::string& mnemonic) {
    std::string lower = StringUtils::ToLower(mnemonic);
    return lower == "call";
}

bool DisassemblyEngine::IsRetInstruction(const std::string& mnemonic) {
    std::string lower = StringUtils::ToLower(mnemonic);
    return lower == "ret" || lower == "retn" || lower == "retf";
}

std::optional<uint64_t> DisassemblyEngine::ExtractBranchTarget(const InstructionInfo& info) {
    // 从操作数中提取分支目标地址
    // 实际实现需要解析操作数格式
    // 这里返回空值,由 x64dbg API 提供的信息填充
    return std::nullopt;
}

} // namespace MCP
