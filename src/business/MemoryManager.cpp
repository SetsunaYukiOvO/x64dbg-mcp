#include "MemoryManager.h"
#include "DebugController.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../utils/StringUtils.h"
#include "../core/X64DBGBridge.h"
#include <algorithm>

namespace MCP {

MemoryManager& MemoryManager::Instance() {
    static MemoryManager instance;
    return instance;
}

std::vector<uint8_t> MemoryManager::Read(uint64_t address, size_t size) {
    if (!DebugController::Instance().IsPaused()) {
        throw DebuggerNotPausedException();
    }
    
    if (size == 0) {
        throw InvalidSizeException("Size cannot be zero");
    }
    
    if (size > MAX_READ_SIZE) {
        throw InvalidSizeException("Read size exceeds maximum: " + 
                                  std::to_string(MAX_READ_SIZE));
    }
    
    // 检查起始地址是否可读
    if (!DbgMemIsValidReadPtr(address)) {
        throw InvalidAddressException("Memory not readable at address: " +
                                     StringUtils::FormatAddress(address));
    }
    
    std::vector<uint8_t> buffer(size);
    
    // 尝试读取完整大小
    if (DbgMemRead(address, buffer.data(), size)) {
        Logger::Trace("Read {} bytes from 0x{:X}", size, address);
        return buffer;
    }
    
    // 如果失败，尝试分块读取（处理跨页边界的情况）
    Logger::Debug("Full read failed, attempting chunked read for {} bytes from 0x{:X}", size, address);
    
    size_t chunkSize = 4096; // 4KB chunks
    size_t totalRead = 0;
    
    for (size_t offset = 0; offset < size; offset += chunkSize) {
        size_t currentChunkSize = std::min(chunkSize, size - offset);
        uint64_t currentAddr = address + offset;
        
        // 检查当前块是否可读
        if (!DbgMemIsValidReadPtr(currentAddr)) {
            Logger::Warning("Memory not readable at chunk starting at 0x{:X}", currentAddr);
            break;
        }
        
        if (DbgMemRead(currentAddr, buffer.data() + offset, currentChunkSize)) {
            totalRead += currentChunkSize;
        } else {
            Logger::Warning("Failed to read chunk at 0x{:X}, size {}", currentAddr, currentChunkSize);
            break;
        }
    }
    
    if (totalRead == 0) {
        throw MCPException("Failed to read memory at: " +
                          StringUtils::FormatAddress(address));
    }
    
    // 如果只读取了部分数据，调整缓冲区大小
    if (totalRead < size) {
        buffer.resize(totalRead);
        Logger::Info("Partial read: {} of {} bytes from 0x{:X}", totalRead, size, address);
    }
    
    return buffer;
}

size_t MemoryManager::Write(uint64_t address, const std::vector<uint8_t>& data) {
    if (!DebugController::Instance().IsPaused()) {
        throw DebuggerNotPausedException();
    }
    
    if (data.empty()) {
        throw InvalidSizeException("Data cannot be empty");
    }
    
    if (data.size() > MAX_WRITE_SIZE) {
        throw InvalidSizeException("Write size exceeds maximum: " +
                                  std::to_string(MAX_WRITE_SIZE));
    }
    
    if (!IsWritable(address, data.size())) {
        throw InvalidAddressException("Memory not writable at address: " +
                                     StringUtils::FormatAddress(address));
    }
    
    if (!DbgMemWrite(address, data.data(), data.size())) {
        throw MCPException("Failed to write memory at: " +
                          StringUtils::FormatAddress(address));
    }
    
    Logger::Debug("Wrote {} bytes to 0x{:X}", data.size(), address);
    return data.size();
}

std::vector<MemorySearchResult> MemoryManager::Search(
    const std::string& pattern,
    uint64_t startAddress,
    uint64_t endAddress,
    size_t maxResults)
{
    if (!DebugController::Instance().IsPaused()) {
        throw DebuggerNotPausedException();
    }
    
    Logger::Debug("Searching memory for pattern: {}", pattern);
    
    // 解析模式
    std::vector<uint8_t> patternBytes;
    std::vector<bool> mask;
    
    // Parse pattern — supports both spaced ("4D 5A 90 00") and continuous ("4D5A9000") hex,
    // as well as wildcards ("48 83 EC ?? 48" or "4883EC??48").
    std::vector<std::string> tokens = StringUtils::Split(pattern, ' ');

    // If pattern has no spaces and looks like continuous hex (with optional ?? wildcards),
    // split it into 2-char byte tokens.
    if (tokens.size() == 1 && tokens[0].size() > 2) {
        std::string raw = StringUtils::Trim(tokens[0]);
        if (raw.size() % 2 == 0) {
            bool looksLikeContinuousHex = true;
            for (size_t i = 0; i < raw.size(); i += 2) {
                char a = raw[i], b = raw[i + 1];
                bool byteOk = (a == '?' && b == '?') ||
                              (std::isxdigit(a) && std::isxdigit(b));
                if (!byteOk) { looksLikeContinuousHex = false; break; }
            }
            if (looksLikeContinuousHex) {
                tokens.clear();
                for (size_t i = 0; i < raw.size(); i += 2) {
                    tokens.push_back(raw.substr(i, 2));
                }
            }
        }
    }

    for (const auto& token : tokens) {
        std::string cleaned = StringUtils::Trim(token);
        if (cleaned.empty()) continue;
        
        if (cleaned == "??" || cleaned == "?") {
            patternBytes.push_back(0);
            mask.push_back(false);  // 通配符
        } else {
            try {
                auto bytes = StringUtils::HexToBytes(cleaned);
                if (bytes.size() != 1) {
                    throw InvalidParamsException(
                        "Invalid pattern token (must be exactly one byte or wildcard): " + cleaned);
                }
                patternBytes.push_back(bytes[0]);
                mask.push_back(true);  // 精确匹配
            } catch (const InvalidParamsException&) {
                throw;
            } catch (...) {
                throw InvalidParamsException("Invalid pattern format: " + cleaned);
            }
        }
    }
    
    if (patternBytes.empty()) {
        throw InvalidParamsException("Empty search pattern");
    }
    
    std::vector<MemorySearchResult> results;
    
    // 获取内存区域
    auto regions = EnumerateRegions();
    
    for (const auto& region : regions) {
        if (results.size() >= maxResults) {
            break;
        }
        
        // 跳过非 MEM_COMMIT 区域
        if (region.type != "MEM_COMMIT") {
            continue;
        }
        
        // 检查地址范围
        uint64_t regionEnd = region.base + region.size;
        if (startAddress > 0 && regionEnd <= startAddress) continue;
        if (endAddress > 0 && region.base >= endAddress) continue;
        
        // 计算实际搜索范围
        uint64_t searchStart = region.base;
        uint64_t searchEnd = regionEnd;
        if (startAddress > 0 && searchStart < startAddress) {
            searchStart = startAddress;
        }
        if (endAddress > 0 && searchEnd > endAddress) {
            searchEnd = endAddress;
        }
        
        // 跳过不可读区域
        if (!IsReadable(searchStart, std::min<size_t>(4096, searchEnd - searchStart))) {
            continue;
        }
        
        Logger::Trace("Searching region: 0x{:X} - 0x{:X}", searchStart, searchEnd);
        
        // 分块读取并搜索
        const size_t chunkSize = 64 * 1024; // 64KB chunks
        for (uint64_t addr = searchStart; addr < searchEnd && results.size() < maxResults; ) {
            size_t readSize = std::min<size_t>(chunkSize, searchEnd - addr);
            if (readSize == 0) break;
            
            try {
                // 读取内存块
                std::vector<uint8_t> buffer(readSize);
                if (!DbgMemRead(addr, buffer.data(), readSize)) {
                    // 读取失败，跳过此块
                    addr += readSize;
                    continue;
                }
                
                // 在内存块中搜索模式
                for (size_t i = 0; i + patternBytes.size() <= buffer.size() && results.size() < maxResults; ++i) {
                    if (MatchPattern(buffer.data() + i, patternBytes, mask)) {
                        MemorySearchResult match;
                        match.address = addr + i;
                        match.data.assign(buffer.begin() + i, 
                                        buffer.begin() + i + patternBytes.size());
                        results.push_back(match);
                        Logger::Trace("Found match at 0x{:X}", match.address);
                    }
                }
                
                addr += readSize;
            } catch (...) {
                // 读取失败，跳过此块
                addr += readSize;
            }
        }
    }
    
    Logger::Info("Found {} matches for pattern", results.size());
    return results;
}

std::optional<MemoryRegion> MemoryManager::GetMemoryInfo(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 获取进程句柄
    HANDLE hProcess = DbgGetProcessHandle();
    if (hProcess == nullptr) {
        Logger::Warning("Failed to get process handle for memory info query");
        return std::nullopt;
    }
    
    // 使用 VirtualQueryEx 获取内存信息
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = VirtualQueryEx(
        hProcess,
        reinterpret_cast<LPCVOID>(address),
        &mbi,
        sizeof(mbi)
    );
    
    if (result == 0) {
        Logger::Debug("VirtualQueryEx failed for address 0x{:X}", address);
        return std::nullopt;
    }
    
    // 构建内存区域信息
    MemoryRegion region;
    region.base = reinterpret_cast<uint64_t>(mbi.BaseAddress);
    region.size = mbi.RegionSize;
    region.protection = ProtectionToString(mbi.Protect);
    
    // 转换内存状态
    switch (mbi.State) {
        case MEM_COMMIT:
            region.type = "MEM_COMMIT";
            break;
        case MEM_RESERVE:
            region.type = "MEM_RESERVE";
            break;
        case MEM_FREE:
            region.type = "MEM_FREE";
            break;
        default:
            region.type = "UNKNOWN";
            break;
    }
    
    // 尝试获取模块名或其他信息（使用 x64dbg API）
    char modName[MAX_MODULE_SIZE] = {0};
    if (DbgGetModuleAt(address, modName)) {
        region.info = modName;
    }
    
    Logger::Debug("GetMemoryInfo for 0x{:X}: base=0x{:X}, size=0x{:X}, type={}", 
                  address, region.base, region.size, region.type);
    
    return region;
}

std::vector<MemoryRegion> MemoryManager::EnumerateRegions() {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<MemoryRegion> regions;
    
    // 使用 x64dbg API 枚举内存区域
    MEMMAP memmap = {0};
    if (!DbgMemMap(&memmap)) {
        Logger::Warning("Failed to enumerate memory regions");
        return regions;
    }
    
    // 遍历所有内存页
    for (int i = 0; i < memmap.count; i++) {
        const MEMPAGE& page = memmap.page[i];
        const MEMORY_BASIC_INFORMATION& mbi = page.mbi;
        
        MemoryRegion region;
        region.base = reinterpret_cast<uint64_t>(mbi.BaseAddress);
        region.size = mbi.RegionSize;
        region.protection = ProtectionToString(mbi.Protect);
        
        // 转换内存类型
        switch (mbi.State) {
            case MEM_COMMIT:
                region.type = "MEM_COMMIT";
                break;
            case MEM_RESERVE:
                region.type = "MEM_RESERVE";
                break;
            case MEM_FREE:
                region.type = "MEM_FREE";
                break;
            default:
                region.type = "UNKNOWN";
                break;
        }
        
        // 使用模块信息（如果有）
        if (page.info[0] != '\0') {
            region.info = page.info;
        }
        
        regions.push_back(region);
    }
    
    // 释放内存映射
    if (memmap.page) {
        BridgeFree(memmap.page);
    }
    
    Logger::Debug("Enumerated {} memory regions", regions.size());
    return regions;
}

bool MemoryManager::IsReadable(uint64_t address, size_t size) {
    // 检查起始地址是否可读
    if (!DbgMemIsValidReadPtr(address)) {
        return false;
    }
    
    // 对于大范围，检查结束地址
    if (size > 4096 && size < MAX_READ_SIZE) {
        uint64_t endAddress = address + size - 1;
        if (!DbgMemIsValidReadPtr(endAddress)) {
            return false;
        }
    }
    
    return true;
}

bool MemoryManager::IsWritable(uint64_t address, size_t size) {
    auto info = GetMemoryInfo(address);
    if (!info.has_value()) {
        return false;
    }
    
    // 检查保护属性是否包含写权限
    bool writable = info->protection.find("WRITE") != std::string::npos ||
                   info->protection.find("EXECUTE_READWRITE") != std::string::npos;
    
    if (!writable) {
        return false;
    }
    
    // 对于大范围，检查结束地址所在页的权限
    if (size > 4096 && size < MAX_WRITE_SIZE) {
        uint64_t endAddress = address + size - 1;
        auto endInfo = GetMemoryInfo(endAddress);
        if (!endInfo.has_value()) {
            return false;
        }
        return endInfo->protection.find("WRITE") != std::string::npos ||
               endInfo->protection.find("EXECUTE_READWRITE") != std::string::npos;
    }
    
    return true;
}

uint64_t MemoryManager::Allocate(size_t size) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 使用 Windows API VirtualAllocEx 在目标进程中分配内存
    HANDLE hProcess = DbgGetProcessHandle();
    if (hProcess == nullptr) {
        Logger::Error("Failed to get process handle for memory allocation");
        throw MCPException("Failed to allocate memory");
    }
    
    LPVOID allocatedAddr = VirtualAllocEx(
        hProcess,
        nullptr,                    // 让系统选择地址
        size,
        MEM_COMMIT | MEM_RESERVE,   // 提交并保留
        PAGE_EXECUTE_READWRITE      // 可读写执行
    );
    
    if (allocatedAddr == nullptr) {
        DWORD error = GetLastError();
        Logger::Error("VirtualAllocEx failed with error code: {}", error);
        throw MCPException("Failed to allocate memory");
    }
    
    Logger::Info("Allocated {} bytes at 0x{:X}", size, reinterpret_cast<uint64_t>(allocatedAddr));
    return reinterpret_cast<uint64_t>(allocatedAddr);
}

bool MemoryManager::Free(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    HANDLE hProcess = DbgGetProcessHandle();
    if (hProcess == nullptr) {
        Logger::Error("Failed to get process handle for memory free");
        return false;
    }
    
    BOOL result = VirtualFreeEx(
        hProcess,
        reinterpret_cast<LPVOID>(address),
        0,
        MEM_RELEASE
    );
    
    if (!result) {
        DWORD error = GetLastError();
        Logger::Error("VirtualFreeEx failed with error code: {}", error);
        return false;
    }
    
    Logger::Info("Freed memory at 0x{:X}", address);
    return true;
}

std::vector<uint8_t> MemoryManager::ParsePattern(const std::string& pattern) {
    // 解析十六进制模式
    return StringUtils::HexToBytes(pattern);
}

bool MemoryManager::MatchPattern(const uint8_t* data, 
                                const std::vector<uint8_t>& pattern,
                                const std::vector<bool>& mask)
{
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (mask[i] && data[i] != pattern[i]) {
            return false;
        }
    }
    return true;
}

std::string MemoryManager::ProtectionToString(uint32_t protect) {
    // Windows 内存保护标志
    switch (protect & 0xFF) {
        case 0x01: return "PAGE_NOACCESS";
        case 0x02: return "PAGE_READONLY";
        case 0x04: return "PAGE_READWRITE";
        case 0x08: return "PAGE_WRITECOPY";
        case 0x10: return "PAGE_EXECUTE";
        case 0x20: return "PAGE_EXECUTE_READ";
        case 0x40: return "PAGE_EXECUTE_READWRITE";
        case 0x80: return "PAGE_EXECUTE_WRITECOPY";
        default: return "UNKNOWN";
    }
}

} // namespace MCP
