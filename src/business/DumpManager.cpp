#include "DumpManager.h"
#include "DebugController.h"
#include "BreakpointManager.h"
#include "MemoryManager.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../utils/StringUtils.h"
#include "../core/X64DBGBridge.h"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <windows.h>

#ifdef XDBG_SDK_AVAILABLE
#include "_scriptapi_module.h"  // For Script::Module::EntryFromAddr
#include <bridgelist.h>
#endif

namespace MCP {
namespace {

std::filesystem::path ToFilesystemPath(const std::string& utf8Path) {
    return std::filesystem::u8path(utf8Path);
}

struct SectionLayout {
    uint32_t virtualAddress = 0;
    uint32_t span = 0;
    uint32_t characteristics = 0;
    std::string name;
};

struct ModuleLayout {
    uint32_t entryRva = 0;
    std::vector<SectionLayout> sections;
};

uint32_t GetSectionSpan(const IMAGE_SECTION_HEADER& section) {
    const uint32_t virtualSize = section.Misc.VirtualSize;
    const uint32_t rawSize = section.SizeOfRawData;
    return std::max(virtualSize, rawSize);
}

std::string GetSectionName(const IMAGE_SECTION_HEADER& section) {
    char name[9] = {0};
    std::memcpy(name, section.Name, sizeof(section.Name));
    return std::string(name);
}

bool IsRvaInSection(uint32_t rva, const SectionLayout& section) {
    return section.span != 0 &&
           rva >= section.virtualAddress &&
           rva < section.virtualAddress + section.span;
}

std::optional<size_t> FindSectionIndex(const ModuleLayout& layout, uint32_t rva) {
    for (size_t i = 0; i < layout.sections.size(); ++i) {
        if (IsRvaInSection(rva, layout.sections[i])) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<ModuleLayout> ReadModuleLayout(uint64_t moduleBase) {
    constexpr size_t kHeaderProbeSize = 0x4000;

    auto& memMgr = MemoryManager::Instance();
    std::vector<uint8_t> peHeader = memMgr.Read(moduleBase, kHeaderProbeSize);
    if (peHeader.size() < sizeof(IMAGE_DOS_HEADER)) {
        return std::nullopt;
    }

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(peHeader.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return std::nullopt;
    }

    if (dosHeader->e_lfanew <= 0 ||
        peHeader.size() < static_cast<size_t>(dosHeader->e_lfanew) + sizeof(IMAGE_NT_HEADERS)) {
        return std::nullopt;
    }

    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        peHeader.data() + dosHeader->e_lfanew
    );
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return std::nullopt;
    }

    const WORD sectionCount = ntHeaders->FileHeader.NumberOfSections;
    if (sectionCount == 0) {
        return std::nullopt;
    }

    const size_t sectionsOffset = static_cast<size_t>(dosHeader->e_lfanew) + sizeof(IMAGE_NT_HEADERS);
    const size_t sectionsSize = static_cast<size_t>(sectionCount) * sizeof(IMAGE_SECTION_HEADER);
    if (peHeader.size() < sectionsOffset + sectionsSize) {
        return std::nullopt;
    }

    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(peHeader.data() + sectionsOffset);

    ModuleLayout layout;
    layout.entryRva = ntHeaders->OptionalHeader.AddressOfEntryPoint;
    layout.sections.reserve(sectionCount);

    for (WORD i = 0; i < sectionCount; ++i) {
        SectionLayout section;
        section.virtualAddress = sections[i].VirtualAddress;
        section.span = GetSectionSpan(sections[i]);
        section.characteristics = sections[i].Characteristics;
        section.name = GetSectionName(sections[i]);
        layout.sections.push_back(section);
    }

    return layout;
}

bool IsLikelyCodeBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return false;
    }

    bool allZero = true;
    bool allInt3 = true;
    for (uint8_t b : bytes) {
        if (b != 0x00) {
            allZero = false;
        }
        if (b != 0xCC) {
            allInt3 = false;
        }
    }

    return !allZero && !allInt3;
}

std::string ToLowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
    );
    return value;
}

std::string CanonicalText(const std::string& value) {
    return ToLowerAscii(StringUtils::FixUtf8Mojibake(value));
}

std::string BaseNameFromPath(const std::string& path) {
    const size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string StripExtension(const std::string& fileName) {
    const size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return fileName;
    }
    return fileName.substr(0, dot);
}

#ifdef XDBG_SDK_AVAILABLE
bool ModuleMatchesQuery(const Script::Module::ModuleInfo& mod, const std::string& query) {
    if (query.empty()) {
        return false;
    }

    const std::string queryLower = CanonicalText(query);
    const std::string name = StringUtils::FixUtf8Mojibake(mod.name);
    const std::string path = StringUtils::FixUtf8Mojibake(mod.path);
    const std::string fileName = BaseNameFromPath(path);

    const std::string nameLower = CanonicalText(name);
    const std::string pathLower = CanonicalText(path);
    const std::string fileLower = CanonicalText(fileName);

    if (queryLower == nameLower || queryLower == pathLower || queryLower == fileLower) {
        return true;
    }

    const bool hasWildcard = queryLower.find('*') != std::string::npos ||
                             queryLower.find('?') != std::string::npos;
    if (hasWildcard) {
        if (StringUtils::WildcardMatchUtf8(queryLower, nameLower) ||
            StringUtils::WildcardMatchUtf8(queryLower, pathLower) ||
            StringUtils::WildcardMatchUtf8(queryLower, fileLower)) {
            return true;
        }
    }

    const std::string queryStemLower = CanonicalText(StripExtension(query));
    if (queryStemLower.empty()) {
        return false;
    }

    const std::string nameStemLower = CanonicalText(StripExtension(name));
    const std::string fileStemLower = CanonicalText(StripExtension(fileName));

    if (hasWildcard) {
        if (StringUtils::WildcardMatchUtf8(queryStemLower, nameStemLower) ||
            StringUtils::WildcardMatchUtf8(queryStemLower, fileStemLower)) {
            return true;
        }
    }

    return queryStemLower == nameStemLower || queryStemLower == fileStemLower;
}

std::optional<uint64_t> ResolveModuleBaseByQueryFallback(const std::string& query) {
    BridgeList<Script::Module::ModuleInfo> moduleList;
    if (!Script::Module::GetList(&moduleList)) {
        return std::nullopt;
    }

    for (size_t i = 0; i < moduleList.Count(); ++i) {
        const auto& mod = moduleList[i];
        if (ModuleMatchesQuery(mod, query)) {
            return mod.base;
        }
    }

    return std::nullopt;
}
#endif

bool SectionContainsRva(const IMAGE_SECTION_HEADER& section, uint32_t rva) {
    const uint32_t start = section.VirtualAddress;
    const uint32_t span = GetSectionSpan(section);
    return span != 0 && rva >= start && rva < start + span;
}

std::optional<uint64_t> FindTransferAddressToTarget(uint64_t moduleBase, uint64_t targetAddress) {
    auto layoutOpt = ReadModuleLayout(moduleBase);
    if (!layoutOpt.has_value()) {
        return std::nullopt;
    }

    const ModuleLayout& layout = layoutOpt.value();
    auto entrySectionIndexOpt = FindSectionIndex(layout, layout.entryRva);
    if (!entrySectionIndexOpt.has_value()) {
        return std::nullopt;
    }

    const auto& entrySection = layout.sections[entrySectionIndexOpt.value()];
    const uint64_t entryVA = moduleBase + layout.entryRva;
    const uint64_t entrySectionEnd =
        moduleBase + static_cast<uint64_t>(entrySection.virtualAddress) + entrySection.span;
    const size_t scanSize = static_cast<size_t>(
        std::min<uint64_t>(0x6000, entrySectionEnd > entryVA ? entrySectionEnd - entryVA : 0)
    );
    if (scanSize < 2) {
        return std::nullopt;
    }

    auto& memMgr = MemoryManager::Instance();
    auto code = memMgr.Read(entryVA, scanSize);

    for (size_t i = 0; i < code.size(); ++i) {
        const uint64_t instructionAddress = entryVA + i;

        if (i + 5 <= code.size() && code[i] == 0xE9) {
            int32_t rel32 = 0;
            std::memcpy(&rel32, code.data() + i + 1, sizeof(rel32));
            const uint64_t target = static_cast<uint64_t>(
                static_cast<int64_t>(instructionAddress) + 5 + rel32
            );
            if (target == targetAddress) {
                return instructionAddress;
            }
        }

        if (i + 2 <= code.size() && code[i] == 0xEB) {
            const int8_t rel8 = static_cast<int8_t>(code[i + 1]);
            const uint64_t target = static_cast<uint64_t>(
                static_cast<int64_t>(instructionAddress) + 2 + rel8
            );
            if (target == targetAddress) {
                return instructionAddress;
            }
        }

        if (i + 6 <= code.size() && code[i] == 0xFF && code[i + 1] == 0x25) {
            int32_t disp32 = 0;
            std::memcpy(&disp32, code.data() + i + 2, sizeof(disp32));

            uint64_t pointerAddress = 0;
#ifdef _WIN64
            pointerAddress = static_cast<uint64_t>(
                static_cast<int64_t>(instructionAddress) + 6 + disp32
            );
#else
            pointerAddress = static_cast<uint32_t>(disp32);
#endif

            try {
                auto pointerBytes = memMgr.Read(pointerAddress, sizeof(duint));
                if (pointerBytes.size() == sizeof(duint)) {
                    duint targetValue = 0;
                    std::memcpy(&targetValue, pointerBytes.data(), sizeof(duint));
                    if (static_cast<uint64_t>(targetValue) == targetAddress) {
                        return instructionAddress;
                    }
                }
            } catch (...) {
                // Ignore unresolved pointer targets.
            }
        }

        // x86: push imm32; ret
        if (i + 6 <= code.size() && code[i] == 0x68 && code[i + 5] == 0xC3) {
            uint32_t imm32 = 0;
            std::memcpy(&imm32, code.data() + i + 1, sizeof(imm32));
            if (static_cast<uint64_t>(imm32) == targetAddress) {
                return instructionAddress;
            }
        }

        // x86/x64: mov reg, imm; jmp reg
        if (i + 7 <= code.size() && code[i] >= 0xB8 && code[i] <= 0xBF &&
            code[i + 5] == 0xFF && code[i + 6] >= 0xE0 && code[i + 6] <= 0xE7) {
            uint32_t imm32 = 0;
            std::memcpy(&imm32, code.data() + i + 1, sizeof(imm32));
            if (static_cast<uint64_t>(imm32) == targetAddress) {
                return instructionAddress;
            }
        }

#ifdef _WIN64
        if (i + 13 <= code.size() && code[i] == 0x48 &&
            code[i + 1] >= 0xB8 && code[i + 1] <= 0xBF &&
            code[i + 10] == 0xFF && code[i + 11] >= 0xE0 && code[i + 11] <= 0xE7) {
            uint64_t imm64 = 0;
            std::memcpy(&imm64, code.data() + i + 2, sizeof(imm64));
            if (imm64 == targetAddress) {
                return instructionAddress;
            }
        }
#endif
    }

    return std::nullopt;
}

bool EnsureDebuggerPausedForDump(const char* phase) {
    auto& debugController = DebugController::Instance();
    if (!debugController.IsDebugging()) {
        return false;
    }

    if (debugController.IsPaused()) {
        return true;
    }

    Logger::Info("Debugger is running before {}. Requesting pause...", phase);
    if (!debugController.Pause()) {
        Logger::Warning("Failed to pause debugger before {}", phase);
        return false;
    }

    return debugController.IsPaused();
}

bool EnsureExecutionInModuleContext(
    const std::string& modulePath,
    uint64_t moduleBase,
    uint64_t moduleSize,
    const char* phase)
{
    if (modulePath.empty() || moduleSize == 0) {
        return false;
    }

    const auto isRipInModule = [&](uint64_t rip) {
        return rip >= moduleBase && rip < moduleBase + moduleSize;
    };

    auto& debugController = DebugController::Instance();

    uint64_t currentRip = 0;
    try {
        currentRip = debugController.GetInstructionPointer();
    } catch (...) {
        currentRip = 0;
    }

    if (isRipInModule(currentRip)) {
        return true;
    }

    std::string escapedPath = modulePath;
    size_t quotePos = 0;
    while ((quotePos = escapedPath.find('"', quotePos)) != std::string::npos) {
        escapedPath.replace(quotePos, 1, "\\\"");
        quotePos += 2;
    }

    const std::string initCommand = "init \"" + escapedPath + "\"";
    Logger::Info(
        "RIP {} is outside module before {}. Resetting context with {}",
        StringUtils::FormatAddress(currentRip),
        phase,
        initCommand
    );

    if (!DbgCmdExec(initCommand.c_str())) {
        Logger::Warning("Failed to execute init command before {}", phase);
        return false;
    }

    // Wait for debugger session to become available after init.
    const auto attachStart = std::chrono::steady_clock::now();
    while (!debugController.IsDebugging()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - attachStart
        ).count();
        if (elapsed >= 5000) {
            Logger::Warning("Debugger did not become active after init command");
            return false;
        }
        Sleep(10);
    }

    // Move execution until debugger lands in target module startup flow.
    for (int attempt = 0; attempt < 12; ++attempt) {
        if (debugController.IsPaused()) {
            uint64_t rip = 0;
            try {
                rip = debugController.GetInstructionPointer();
            } catch (...) {
                rip = 0;
            }

            if (isRipInModule(rip)) {
                Logger::Info("Recovered module context at RIP {}", StringUtils::FormatAddress(rip));
                return true;
            }
        }

        // Continue execution and wait for the next debug stop.
        // This matches manual recovery flow: init -> run -> stop in loader/module.
        if (!debugController.Run()) {
            Logger::Warning("Run command was not accepted during context recovery attempt {}", attempt + 1);
            Sleep(50);
        }

        // Run command is asynchronous; give debugger state a short time to transition.
        Sleep(50);

        const auto waitStart = std::chrono::steady_clock::now();
        while (!debugController.IsPaused()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - waitStart
            ).count();

            if (elapsed >= 15000) {
                Logger::Warning("Context recovery wait timed out on attempt {}", attempt + 1);
                break;
            }

            Sleep(10);
        }

        if (!debugController.IsPaused()) {
            // Try to force a break and continue recovery attempts.
            try {
                debugController.Pause();
            } catch (...) {
                // Ignore pause errors here; next attempt may still recover.
            }
        }
    }

    Logger::Warning("Failed to recover module context before {}", phase);
    return false;
}

} // namespace

DumpManager& DumpManager::Instance() {
    static DumpManager instance;
    return instance;
}

DumpResult DumpManager::DumpModule(
    const std::string& moduleNameOrAddress,
    const std::string& outputPath,
    const DumpOptions& options,
    ProgressCallback progressCallback)
{
    DumpResult result;
    DumpProgress progress;
    
    auto updateProgress = [&](DumpProgress::Stage stage, int percent, const std::string& msg) {
        progress.stage = stage;
        progress.progress = percent;
        progress.message = msg;
        if (progressCallback) {
            progressCallback(progress);
        }
        Logger::Info("[Dump] {} - {}%: {}", static_cast<int>(stage), percent, msg);
    };
    
    try {
        if (!DebugController::Instance().IsDebugging()) {
            throw DebuggerNotRunningException();
        }

        if (!EnsureDebuggerPausedForDump("module dump")) {
            throw MCPException("Failed to pause debugger before dump");
        }
        
        updateProgress(DumpProgress::Stage::Preparing, 0, "Parsing module information");
        
        // 瑙ｆ瀽妯″潡鍦板潃
        auto moduleBaseOpt = ParseModuleOrAddress(moduleNameOrAddress);
        if (!moduleBaseOpt.has_value()) {
            throw InvalidParamsException("Invalid module name or address: " + moduleNameOrAddress);
        }
        
        uint64_t moduleBase = moduleBaseOpt.value();
        uint64_t moduleSize = GetModuleSize(moduleBase);
        uint64_t entryPoint = GetModuleEntryPoint(moduleBase);
        
        if (moduleSize == 0) {
            throw MCPException("Failed to get module size");
        }
        
        Logger::Info("Dumping module at {}, size: {} bytes, EP: {}",
                    StringUtils::FormatAddress(moduleBase),
                    moduleSize,
                    StringUtils::FormatAddress(entryPoint));

        const std::string modulePath = GetModulePath(moduleBase);
        const std::filesystem::path moduleFsPath =
            modulePath.empty() ? std::filesystem::path() : ToFilesystemPath(modulePath);
        const std::filesystem::path outputFsPath = ToFilesystemPath(outputPath);
        std::string packerId;
        const bool isPackedImage = IsPacked(moduleBase, packerId);
        const bool hasResolvedOEP =
            options.forcedOEP.has_value() && options.forcedOEP.value() != entryPoint;

        // If still packed and no resolved OEP is provided, return a runnable baseline by copying
        // the original image instead of writing unstable runtime memory state.
        if (isPackedImage && !options.autoDetectOEP && !hasResolvedOEP &&
            !modulePath.empty() && std::filesystem::exists(moduleFsPath)) {
            updateProgress(DumpProgress::Stage::Preparing, 5,
                           "Packed module fallback: copying original image");

            std::filesystem::copy_file(
                moduleFsPath,
                outputFsPath,
                std::filesystem::copy_options::overwrite_existing
            );

            result.success = true;
            result.filePath = outputPath;
            result.dumpedSize = std::filesystem::file_size(outputFsPath);
            result.originalEP = entryPoint;
            result.newEP = entryPoint;

            updateProgress(
                DumpProgress::Stage::Completed,
                100,
                "Packed image copied. Run this file or resolve OEP first for true unpack dump."
            );
            progress.success = true;
            result.finalProgress = progress;

            Logger::Warning(
                "Packed module '{}' copied to output because no resolved OEP was provided",
                packerId
            );
            return result;
        }
        
        updateProgress(DumpProgress::Stage::ReadingMemory, 10, "Reading module memory");
        
        // 璇诲彇鏁翠釜妯″潡鍐呭瓨
        auto& memMgr = MemoryManager::Instance();
        std::vector<uint8_t> buffer;
        
        if (options.dumpFullImage) {
            // 鎸塒E鏂囦欢澶у皬dump
            buffer = memMgr.Read(moduleBase, moduleSize);
        } else {
            // 鍙猟ump宸叉彁浜ょ殑鍐呭瓨椤?
            buffer = memMgr.Read(moduleBase, moduleSize);
        }
        
        result.dumpedSize = buffer.size();
        result.originalEP = entryPoint;
        
        // 楠岃瘉PE澶?
        if (!ValidatePEHeader(buffer)) {
            Logger::Warning("Invalid PE header detected, attempting to continue...");
        }
        
        updateProgress(DumpProgress::Stage::FixingPEHeaders, 30, "Fixing PE headers");
        
        // Rebuild PE header.
        if (options.rebuildPE) {
            std::optional<uint32_t> newOEP;

            const auto trySetOEP = [&](uint64_t absoluteOEP, const char* source) {
                if (absoluteOEP < moduleBase || absoluteOEP >= moduleBase + moduleSize) {
                    Logger::Warning("{} OEP {} is outside module range [{}, {})",
                                    source,
                                    StringUtils::FormatAddress(absoluteOEP),
                                    StringUtils::FormatAddress(moduleBase),
                                    StringUtils::FormatAddress(moduleBase + moduleSize));
                    return false;
                }

                const uint64_t rva64 = absoluteOEP - moduleBase;
                if (rva64 > std::numeric_limits<uint32_t>::max()) {
                    Logger::Warning("{} OEP RVA {} exceeds 32-bit PE limit",
                                    source,
                                    StringUtils::FormatAddress(rva64));
                    return false;
                }

                newOEP = static_cast<uint32_t>(rva64);
                result.newEP = absoluteOEP;
                Logger::Info("{} OEP: {} (RVA: {})",
                             source,
                             StringUtils::FormatAddress(absoluteOEP),
                             StringUtils::FormatAddress(newOEP.value()));
                return true;
            };

            if (options.forcedOEP.has_value()) {
                if (!trySetOEP(options.forcedOEP.value(), "Forced")) {
                    throw InvalidParamsException("Forced OEP is outside target module range");
                }
            } else if (options.autoDetectOEP) {
                auto detectedOEP = DetectOEP(moduleBase);
                if (detectedOEP.has_value()) {
                    trySetOEP(detectedOEP.value(), "Auto-detected");
                }
            } else if (options.fixOEP) {
                trySetOEP(entryPoint, "Entry-point");
            }
            
            if (!RebuildPEHeaders(moduleBase, buffer, newOEP)) {
                Logger::Warning("Failed to rebuild PE headers");
            }
        }

        // 绉婚櫎PE鏍￠獙鍜?
        if (options.removeIntegrityCheck) {
            FixPEChecksum(buffer);
        }
        
        updateProgress(DumpProgress::Stage::Writing, 90, "Writing to file");
        
        // 鍐欏叆鏂囦欢
        std::ofstream outFile(outputFsPath, std::ios::binary);
        if (!outFile) {
            throw MCPException("Failed to create output file: " + outputPath);
        }
        
        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        outFile.close();
        
        if (!outFile.good()) {
            throw MCPException("Failed to write dump file");
        }
        
        result.success = true;
        result.filePath = outputPath;
        
        updateProgress(DumpProgress::Stage::Completed, 100, "Dump completed successfully");
        progress.success = true;
        result.finalProgress = progress;
        
        Logger::Info("Module dumped successfully to: {}", outputPath);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
        progress.stage = DumpProgress::Stage::Failed;
        progress.success = false;
        progress.message = e.what();
        result.finalProgress = progress;
        
        Logger::Error("Dump failed: {}", e.what());
    }
    
    return result;
}

DumpResult DumpManager::DumpMemoryRegion(
    uint64_t startAddress,
    size_t size,
    const std::string& outputPath,
    bool asRawBinary)
{
    DumpResult result;
    
    try {
        if (!DebugController::Instance().IsDebugging()) {
            throw DebuggerNotRunningException();
        }
        
        Logger::Info("Dumping memory region {} - {} ({} bytes)",
                    StringUtils::FormatAddress(startAddress),
                    StringUtils::FormatAddress(startAddress + size),
                    size);
        
        auto& memMgr = MemoryManager::Instance();
        std::vector<uint8_t> buffer = memMgr.Read(startAddress, size);
        
        if (!asRawBinary && ValidatePEHeader(buffer)) {
            // 灏濊瘯淇PE
            Logger::Info("PE header detected, attempting to fix");
            RebuildPEHeaders(startAddress, buffer);
        }
        
        std::ofstream outFile(ToFilesystemPath(outputPath), std::ios::binary);
        if (!outFile) {
            throw MCPException("Failed to create output file: " + outputPath);
        }
        
        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        outFile.close();
        
        result.success = true;
        result.filePath = outputPath;
        result.dumpedSize = buffer.size();
        result.finalProgress.stage = DumpProgress::Stage::Completed;
        result.finalProgress.progress = 100;
        result.finalProgress.message = "Memory region dumped successfully";
        result.finalProgress.success = true;
        
        Logger::Info("Memory region dumped successfully to: {}", outputPath);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
        result.finalProgress.stage = DumpProgress::Stage::Failed;
        result.finalProgress.progress = 0;
        result.finalProgress.message = e.what();
        result.finalProgress.success = false;
        Logger::Error("Memory dump failed: {}", e.what());
    }

    return result;
}

ModuleDumpInfo DumpManager::AnalyzeModule(const std::string& moduleNameOrAddress) {
    ModuleDumpInfo info;

    try {
        if (!DebugController::Instance().IsDebugging()) {
            throw DebuggerNotRunningException();
        }

        if (!EnsureDebuggerPausedForDump("module analysis")) {
            throw MCPException("Failed to pause debugger before module analysis");
        }

        auto moduleBaseOpt = ParseModuleOrAddress(moduleNameOrAddress);
        if (!moduleBaseOpt.has_value()) {
            throw InvalidParamsException("Invalid module");
        }

        uint64_t moduleBase = moduleBaseOpt.value();
        info.baseAddress = moduleBase;
        info.size = GetModuleSize(moduleBase);
        info.entryPoint = GetModuleEntryPoint(moduleBase);
        info.path = GetModulePath(moduleBase);

        size_t lastSlash = info.path.find_last_of("\\/");
        info.name = (lastSlash != std::string::npos) ?
                    info.path.substr(lastSlash + 1) : info.path;

        info.isPacked = IsPacked(moduleBase, info.packerId);

    } catch (const std::exception& e) {
        Logger::Error("Failed to analyze module: {}", e.what());
        throw;
    }

    return info;
}

std::optional<uint64_t> DumpManager::DetectOEP(uint64_t moduleBase, const std::string& strategy) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }

    if (!EnsureDebuggerPausedForDump("OEP detection")) {
        throw MCPException("Failed to pause debugger before OEP detection");
    }

    (void)strategy; // only code_analysis is implemented

    Logger::Debug("Detecting OEP for module at {}",
                  StringUtils::FormatAddress(moduleBase));

    auto result = DetectOEPByPattern(moduleBase);
    if (result.has_value()) {
        Logger::Info("OEP detected: {}", StringUtils::FormatAddress(result.value()));
    } else {
        Logger::Warning("OEP detection failed for module at {}",
                        StringUtils::FormatAddress(moduleBase));
    }
    return result;
}

std::vector<MemoryRegionDump> DumpManager::GetDumpableRegions(uint64_t moduleBase) {
    std::vector<MemoryRegionDump> regions;
    
    auto& memMgr = MemoryManager::Instance();
    auto allRegions = memMgr.EnumerateRegions();
    
    for (const auto& region : allRegions) {
        // 杩囨护鏉′欢
        if (moduleBase != 0 && region.base < moduleBase) {
            continue;
        }
        
        if (moduleBase != 0) {
            uint64_t moduleSize = GetModuleSize(moduleBase);
            if (region.base >= moduleBase + moduleSize) {
                continue;
            }
        }
        
        // 鍙寘鍚凡鎻愪氦鐨勫彲璇诲唴瀛?
        if (region.type.find("MEM_COMMIT") == std::string::npos) {
            continue;
        }
        
        MemoryRegionDump dumpRegion;
        dumpRegion.address = region.base;
        dumpRegion.size = region.size;
        dumpRegion.protection = region.protection;
        dumpRegion.type = region.type;
        dumpRegion.name = region.info;
        
        regions.push_back(dumpRegion);
    }
    
    Logger::Debug("Found {} dumpable regions for module at {}",
                  regions.size(),
                  StringUtils::FormatAddress(moduleBase));
    return regions;
}

bool DumpManager::RebuildPEHeaders(uint64_t moduleBase, std::vector<uint8_t>& buffer,
                                   std::optional<uint32_t> newEP) {
    try {
        if (buffer.size() < sizeof(IMAGE_DOS_HEADER)) {
            return false;
        }
        
        auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer.data());
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }
        
        if (buffer.size() < dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS)) {
            return false;
        }
        
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.data() + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }
        
        // 淇鍏ュ彛鐐?
        if (newEP.has_value()) {
            ntHeaders->OptionalHeader.AddressOfEntryPoint = newEP.value();
            Logger::Info("Updated entry point to RVA: {}",
                         StringUtils::FormatAddress(newEP.value()));
        }
        
        // 淇ImageBase
        // Use PE-standard default ImageBase instead of ASLR runtime address
#ifdef XDBG_ARCH_X64
        ntHeaders->OptionalHeader.ImageBase = 0x0000000140000000ULL;
#else
        ntHeaders->OptionalHeader.ImageBase = 0x00400000UL;
#endif

        // 瀵归綈鑺?
        AlignPESections(buffer);
        
        Logger::Info("PE headers rebuilt successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Failed to rebuild PE headers: {}", e.what());
        return false;
    }
}

void DumpManager::SetOEPDetectionStrategy(
    std::function<std::optional<uint64_t>(uint64_t)> strategy) {
    m_oepDetectionStrategy = strategy;
    Logger::Info("Custom OEP detection strategy set");
}

// ========== 绉佹湁杈呭姪鏂规硶 ==========

std::optional<uint64_t> DumpManager::ParseModuleOrAddress(const std::string& input) {
    // 灏濊瘯浣滀负鍦板潃瑙ｆ瀽
    try {
        uint64_t addr = StringUtils::ParseAddress(input);
        if (addr != 0) {
            return addr;
        }
    } catch (...) {
        // 涓嶆槸鍦板潃,灏濊瘯浣滀负妯″潡鍚?
    }
    
    // 浣滀负妯″潡鍚嶆煡鎵?
    char szModPath[MAX_PATH] = {0};
    duint modBase = DbgFunctions()->ModBaseFromName(input.c_str());
    
    if (modBase != 0) {
        return modBase;
    }

#ifdef XDBG_SDK_AVAILABLE
    auto fallbackBase = ResolveModuleBaseByQueryFallback(input);
    if (fallbackBase.has_value()) {
        return fallbackBase.value();
    }
#endif
    
    return std::nullopt;
}

bool DumpManager::ValidatePEHeader(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }
    
    auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(buffer.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    
    if (buffer.size() < dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS)) {
        return false;
    }
    
    auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(buffer.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    
    return true;
}

bool DumpManager::IsPacked(uint64_t moduleBase, std::string& packerId) {
    try {
        auto& memMgr = MemoryManager::Instance();

        // Read PE header from module memory.
        std::vector<uint8_t> peHeader = memMgr.Read(moduleBase, 4096);

        if (!ValidatePEHeader(peHeader)) {
            return false;
        }

        auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(peHeader.data());
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(peHeader.data() + dosHeader->e_lfanew);

        const WORD sectionCount = ntHeaders->FileHeader.NumberOfSections;
        if (sectionCount < 2) {
            packerId = "Packed-like (few sections)";
            return true;
        }

        auto* sections = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            reinterpret_cast<uint8_t*>(ntHeaders) + sizeof(IMAGE_NT_HEADERS)
        );

        const uint32_t entryRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        int entrySection = -1;

        bool hasExecutableZeroRawSection = false;
        bool hasMarkerSectionName = false;
        std::string markerSectionName;

        for (int i = 0; i < sectionCount; i++) {
            if (SectionContainsRva(sections[i], entryRVA)) {
                entrySection = i;
            }

            const uint32_t sectionSpan = GetSectionSpan(sections[i]);
            if (sections[i].SizeOfRawData == 0 &&
                sectionSpan >= 0x2000 &&
                (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
                hasExecutableZeroRawSection = true;
            }

            const std::string sectionNameLower = ToLowerAscii(GetSectionName(sections[i]));
            static const std::vector<std::string> kPackedMarkers = {
                "upx", "aspack", "petite", "pec", "themida", "vmp", "mpress", "fsg", "enigma"
            };
            for (const auto& marker : kPackedMarkers) {
                if (sectionNameLower.find(marker) != std::string::npos) {
                    hasMarkerSectionName = true;
                    markerSectionName = sectionNameLower;
                    break;
                }
            }

            if (hasMarkerSectionName) {
                break;
            }
        }

        const IMAGE_DATA_DIRECTORY& importDir =
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        const bool importLooksMissing =
            importDir.VirtualAddress == 0 ||
            importDir.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR);

        int suspiciousScore = 0;
        if (entrySection < 0) {
            suspiciousScore += 2;
        } else {
            const IMAGE_SECTION_HEADER& epSection = sections[entrySection];
            if (entrySection == sectionCount - 1) {
                suspiciousScore += 1;
            }

            const bool epWritable = (epSection.Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
            const bool epExecutable = (epSection.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            if (epWritable && epExecutable) {
                suspiciousScore += 1;
            }

            if (epSection.SizeOfRawData == 0 && GetSectionSpan(epSection) >= 0x1000) {
                suspiciousScore += 1;
            }
        }

        if (importLooksMissing) {
            suspiciousScore += 1;
        }
        if (hasExecutableZeroRawSection) {
            suspiciousScore += 1;
        }
        if (hasMarkerSectionName) {
            suspiciousScore += 2;
        }

        if (suspiciousScore >= 2) {
            if (hasMarkerSectionName) {
                packerId = "Packed-like (section marker: " + markerSectionName + ")";
            } else {
                packerId = "Packed-like layout";
            }
            return true;
        }

        packerId = "";
        return false;

    } catch (const std::exception& e) {
        Logger::Error("Packer detection failed: {}", e.what());
        return false;
    }
}

uint64_t DumpManager::GetModuleSize(uint64_t moduleBase) {
    duint size = DbgFunctions()->ModSizeFromAddr(moduleBase);
    return static_cast<uint64_t>(size);
}

uint64_t DumpManager::GetModuleEntryPoint(uint64_t moduleBase) {
    // Use Script API to get entry point
    duint entry = Script::Module::EntryFromAddr(moduleBase);
    return static_cast<uint64_t>(entry);
}

std::string DumpManager::GetModulePath(uint64_t moduleBase) {
    char path[MAX_PATH] = {0};
    if (DbgFunctions()->ModPathFromAddr(moduleBase, path, MAX_PATH)) {
        return StringUtils::FixUtf8Mojibake(std::string(path));
    }
    return "";
}

std::optional<uint64_t> DumpManager::DetectOEPByPattern(uint64_t moduleBase) {
    try {
        auto& memMgr = MemoryManager::Instance();
        const uint64_t moduleSize = GetModuleSize(moduleBase);

        auto layoutOpt = ReadModuleLayout(moduleBase);
        if (layoutOpt.has_value()) {
            const ModuleLayout& layout = layoutOpt.value();
            auto entrySectionIndexOpt = FindSectionIndex(layout, layout.entryRva);

            if (entrySectionIndexOpt.has_value()) {
                const size_t entrySectionIndex = entrySectionIndexOpt.value();
                const auto& entrySection = layout.sections[entrySectionIndex];

                const uint64_t entryVA = moduleBase + layout.entryRva;
                const uint64_t entrySectionEnd =
                    moduleBase + static_cast<uint64_t>(entrySection.virtualAddress) + entrySection.span;
                const size_t scanSize = static_cast<size_t>(
                    std::min<uint64_t>(0x6000, entrySectionEnd > entryVA ? entrySectionEnd - entryVA : 0)
                );

                if (scanSize >= 2) {
                    auto code = memMgr.Read(entryVA, scanSize);

                    const auto isValidTarget = [&](uint64_t target) -> bool {
                        if (target < moduleBase || target >= moduleBase + moduleSize) {
                            return false;
                        }

                        const uint64_t rva64 = target - moduleBase;
                        if (rva64 > std::numeric_limits<uint32_t>::max()) {
                            return false;
                        }

                        auto targetSectionIndexOpt = FindSectionIndex(layout, static_cast<uint32_t>(rva64));
                        if (!targetSectionIndexOpt.has_value()) {
                            return false;
                        }

                        const auto& targetSection = layout.sections[targetSectionIndexOpt.value()];
                        if ((targetSection.characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) {
                            return false;
                        }

                        return targetSectionIndexOpt.value() != entrySectionIndex;
                    };

                    for (size_t i = 0; i < code.size(); ++i) {
                        const uint64_t instructionAddress = entryVA + i;

                        if (i + 5 <= code.size() && code[i] == 0xE9) {
                            int32_t rel32 = 0;
                            std::memcpy(&rel32, code.data() + i + 1, sizeof(rel32));
                            const uint64_t target = static_cast<uint64_t>(
                                static_cast<int64_t>(instructionAddress) + 5 + rel32
                            );
                            if (isValidTarget(target)) {
                                Logger::Info(
                                    "OEP candidate found by near jump at {} -> {}",
                                    StringUtils::FormatAddress(instructionAddress),
                                    StringUtils::FormatAddress(target)
                                );
                                return target;
                            }
                        }

                        if (i + 2 <= code.size() && code[i] == 0xEB) {
                            const int8_t rel8 = static_cast<int8_t>(code[i + 1]);
                            const uint64_t target = static_cast<uint64_t>(
                                static_cast<int64_t>(instructionAddress) + 2 + rel8
                            );
                            if (isValidTarget(target)) {
                                Logger::Info(
                                    "OEP candidate found by short jump at {} -> {}",
                                    StringUtils::FormatAddress(instructionAddress),
                                    StringUtils::FormatAddress(target)
                                );
                                return target;
                            }
                        }

                        if (i + 6 <= code.size() && code[i] == 0xFF && code[i + 1] == 0x25) {
                            int32_t disp32 = 0;
                            std::memcpy(&disp32, code.data() + i + 2, sizeof(disp32));

                            uint64_t pointerAddress = 0;
#ifdef _WIN64
                            pointerAddress = static_cast<uint64_t>(
                                static_cast<int64_t>(instructionAddress) + 6 + disp32
                            );
#else
                            pointerAddress = static_cast<uint32_t>(disp32);
#endif

                            try {
                                auto pointerBytes = memMgr.Read(pointerAddress, sizeof(duint));
                                if (pointerBytes.size() == sizeof(duint)) {
                                    duint targetValue = 0;
                                    std::memcpy(&targetValue, pointerBytes.data(), sizeof(duint));
                                    const uint64_t target = static_cast<uint64_t>(targetValue);
                                    if (isValidTarget(target)) {
                                        Logger::Info(
                                            "OEP candidate found by indirect jump at {} -> {}",
                                            StringUtils::FormatAddress(instructionAddress),
                                            StringUtils::FormatAddress(target)
                                        );
                                        return target;
                                    }
                                }
                            } catch (...) {
                                // Ignore unresolved indirect jump pointers.
                            }
                        }

                        // x86: push imm32; ret
                        if (i + 6 <= code.size() && code[i] == 0x68 && code[i + 5] == 0xC3) {
                            uint32_t imm32 = 0;
                            std::memcpy(&imm32, code.data() + i + 1, sizeof(imm32));
                            const uint64_t target = static_cast<uint64_t>(imm32);
                            if (isValidTarget(target)) {
                                Logger::Info(
                                    "OEP candidate found by push-ret transfer at {} -> {}",
                                    StringUtils::FormatAddress(instructionAddress),
                                    StringUtils::FormatAddress(target)
                                );
                                return target;
                            }
                        }

                        // x86/x64: mov reg, imm; jmp reg
                        if (i + 7 <= code.size() && code[i] >= 0xB8 && code[i] <= 0xBF &&
                            code[i + 5] == 0xFF && code[i + 6] >= 0xE0 && code[i + 6] <= 0xE7) {
                            uint32_t imm32 = 0;
                            std::memcpy(&imm32, code.data() + i + 1, sizeof(imm32));
                            const uint64_t target = static_cast<uint64_t>(imm32);
                            if (isValidTarget(target)) {
                                Logger::Info(
                                    "OEP candidate found by mov-jmp transfer at {} -> {}",
                                    StringUtils::FormatAddress(instructionAddress),
                                    StringUtils::FormatAddress(target)
                                );
                                return target;
                            }
                        }

#ifdef _WIN64
                        if (i + 13 <= code.size() && code[i] == 0x48 &&
                            code[i + 1] >= 0xB8 && code[i + 1] <= 0xBF &&
                            code[i + 10] == 0xFF && code[i + 11] >= 0xE0 && code[i + 11] <= 0xE7) {
                            uint64_t imm64 = 0;
                            std::memcpy(&imm64, code.data() + i + 2, sizeof(imm64));
                            const uint64_t target = imm64;
                            if (isValidTarget(target)) {
                                Logger::Info(
                                    "OEP candidate found by movabs-jmp transfer at {} -> {}",
                                    StringUtils::FormatAddress(instructionAddress),
                                    StringUtils::FormatAddress(target)
                                );
                                return target;
                            }
                        }
#endif
                    }
                }
            }
        }

        std::vector<std::string> patterns = {
            "55 8B EC",
            "55 48 8B EC",
            "48 89 5C 24",
            "40 53",
        };

        const uint64_t searchStart = moduleBase + 0x1000;
        const uint64_t searchEnd = moduleBase + std::min(moduleSize, static_cast<uint64_t>(0x200000));
        if (searchStart < searchEnd) {
            for (const auto& pattern : patterns) {
                auto results = memMgr.Search(pattern, searchStart, searchEnd, 1);
                if (!results.empty()) {
                    auto codeBytes = memMgr.Read(results[0].address, 16);
                    if (!IsLikelyCodeBytes(codeBytes)) {
                        continue;
                    }

                    Logger::Info(
                        "OEP candidate found by function pattern '{}' at {}",
                        pattern,
                        StringUtils::FormatAddress(results[0].address)
                    );
                    return results[0].address;
                }
            }
        }

    } catch (const std::exception& e) {
        Logger::Error("Pattern-based OEP detection failed: {}", e.what());
    }

    return std::nullopt;
}

bool DumpManager::FixPEChecksum(std::vector<uint8_t>& buffer) {
    try {
        if (buffer.size() < sizeof(IMAGE_DOS_HEADER)) {
            return false;
        }
        
        auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer.data());
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }
        
        if (buffer.size() < dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS)) {
            return false;
        }
        
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.data() + dosHeader->e_lfanew);
        
        // 绠€鍗曞湴娓呴浂鏍￠獙鍜?
        ntHeaders->OptionalHeader.CheckSum = 0;
        
        Logger::Debug("PE checksum cleared");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Failed to fix PE checksum: {}", e.what());
        return false;
    }
}

bool DumpManager::AlignPESections(std::vector<uint8_t>& buffer) {
    try {
        if (!ValidatePEHeader(buffer)) {
            return false;
        }
        
        auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer.data());
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.data() + dosHeader->e_lfanew);
        
        uint32_t fileAlignment = ntHeaders->OptionalHeader.FileAlignment;
        if (fileAlignment == 0) {
            fileAlignment = 0x200;
        }
        
        auto* sections = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            reinterpret_cast<uint8_t*>(ntHeaders) + sizeof(IMAGE_NT_HEADERS)
        );

        const auto alignUp = [](uint32_t value, uint32_t alignment) -> uint32_t {
            if (alignment == 0) {
                return value;
            }
            return ((value + alignment - 1) / alignment) * alignment;
        };
        
        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
            const uint32_t virtualAddress = sections[i].VirtualAddress;
            uint32_t virtualSize = sections[i].Misc.VirtualSize;
            if (virtualSize == 0) {
                virtualSize = sections[i].SizeOfRawData;
            }

            // Dump buffer is in memory-image layout, so raw data must point to RVA.
            sections[i].PointerToRawData = virtualAddress;
            sections[i].SizeOfRawData = alignUp(virtualSize, fileAlignment);

            if (virtualAddress >= buffer.size()) {
                sections[i].PointerToRawData = 0;
                sections[i].SizeOfRawData = 0;
                continue;
            }

            const size_t maxAvailable = buffer.size() - static_cast<size_t>(virtualAddress);
            if (sections[i].SizeOfRawData > maxAvailable) {
                sections[i].SizeOfRawData = static_cast<uint32_t>(maxAvailable);
            }
        }
        
        Logger::Debug("PE sections aligned");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Failed to align PE sections: {}", e.what());
        return false;
    }
}

} // namespace MCP

