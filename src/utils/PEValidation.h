#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace MCP {
struct PELayout {
    size_t ntOffset;
    size_t sectionsOffset;
};

// Validate every range before exposing native PE headers to dump consumers.
inline std::optional<PELayout> ValidatePELayout(const std::vector<uint8_t>& data) {
    const auto contains = [&](size_t offset, size_t size) {
        return offset <= data.size() && size <= data.size() - offset;
    };
    IMAGE_DOS_HEADER dos{};
    if (!contains(0, sizeof(dos))) return std::nullopt;
    std::memcpy(&dos, data.data(), sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < sizeof(dos))
        return std::nullopt;
    const size_t ntOffset = static_cast<size_t>(dos.e_lfanew);
    if (ntOffset % alignof(IMAGE_NT_HEADERS) != 0 ||
        !contains(ntOffset, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)))
        return std::nullopt;
    DWORD signature{};
    IMAGE_FILE_HEADER file{};
    std::memcpy(&signature, data.data() + ntOffset, sizeof(signature));
    std::memcpy(&file, data.data() + ntOffset + sizeof(DWORD), sizeof(file));
    const size_t optionalOffset = ntOffset + sizeof(DWORD) + sizeof(file);
    if (signature != IMAGE_NT_SIGNATURE ||
        file.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER) ||
        !contains(optionalOffset, file.SizeOfOptionalHeader)) return std::nullopt;
    IMAGE_OPTIONAL_HEADER optional{};
    std::memcpy(&optional, data.data() + optionalOffset, sizeof(optional));
    if (optional.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ||
        optional.NumberOfRvaAndSizes > IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
        return std::nullopt;
    const size_t sectionsOffset = optionalOffset + file.SizeOfOptionalHeader;
    if (sectionsOffset % alignof(IMAGE_SECTION_HEADER) != 0 ||
        file.NumberOfSections == 0 ||
        !contains(sectionsOffset, size_t(file.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER)))
        return std::nullopt;
    return PELayout{ntOffset, sectionsOffset};
}
} // namespace MCP
