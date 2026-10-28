#include "utils/PEValidation.h"
#include <limits>

int main() {
    std::vector<uint8_t> data(1024);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(data.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 128;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(data.data() + 128);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER);
    nt->FileHeader.NumberOfSections = 1;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR_MAGIC;
    if (!MCP::ValidatePELayout(data)) return 1;
    const auto valid = data;
    for (LONG offset : {-1L, (std::numeric_limits<LONG>::max)(), 1020L, 65L}) {
        dos->e_lfanew = offset;
        if (MCP::ValidatePELayout(data)) return 2;
    }
    data = valid;
    nt = reinterpret_cast<IMAGE_NT_HEADERS*>(data.data() + 128);
    nt->FileHeader.NumberOfSections = 0xffff;
    if (MCP::ValidatePELayout(data)) return 3;
    nt->FileHeader.NumberOfSections = 1;
    nt->FileHeader.SizeOfOptionalHeader = 1;
    if (MCP::ValidatePELayout(data)) return 4;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER) + 8;
    auto layout = MCP::ValidatePELayout(data);
    if (!layout || layout->sectionsOffset != 128 + sizeof(IMAGE_NT_HEADERS) + 8) return 5;
    data.resize(layout->sectionsOffset + sizeof(IMAGE_SECTION_HEADER) - 1);
    if (MCP::ValidatePELayout(data)) return 6;
    return 0;
}
