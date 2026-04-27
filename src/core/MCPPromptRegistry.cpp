/**
 * @file MCPPromptRegistry.cpp
 * @brief MCP Prompt Registry Implementation
 */

#include "MCPPromptRegistry.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace MCP {

json MCPPromptArgument::ToSchema() const {
    json schema;
    schema["name"] = name;
    schema["description"] = description;
    schema["required"] = required;
    return schema;
}

json MCPPromptDefinition::ToMCPFormat() const {
    json arguments_array = json::array();
    for (const auto& arg : arguments) {
        arguments_array.push_back(arg.ToSchema());
    }
    
    return json{
        {"name", name},
        {"description", description},
        {"arguments", arguments_array}
    };
}

std::string MCPPromptDefinition::GeneratePrompt(const json& args) const {
    std::ostringstream oss;
    
    // Generate prompt based on template name
    if (name == "analyze-crash") {
        std::string crash_address = args.value("crash_address", "current location");
        oss << "Perform a systematic crash analysis at: " << crash_address << "\n\n"
            << "## Phase 1: Crash Context\n"
            << "1. Use `debug_get_state` to confirm the debugger is paused at an exception.\n"
            << "2. Use `register_list` to capture ALL register values. Check for:\n"
            << "   - RIP/EIP: the faulting instruction address\n"
            << "   - Registers with suspicious values (0x0, 0xDEADBEEF, 0xCCCCCCCC, 0xFEEEFEEE)\n"
            << "3. Use `disassembly_at` at the crash address with count=30 to see the faulting instruction and context.\n"
            << "4. Use `stack_get_trace` for the full call stack.\n\n"
            << "## Phase 2: Memory Analysis\n"
            << "5. For each register used as a pointer in the faulting instruction:\n"
            << "   - Use `memory_get_info` to check if the memory region is valid.\n"
            << "   - If valid, use `memory_read` (64-128 bytes) to inspect content.\n"
            << "6. Use `memory_read` on the stack area around RSP (~256 bytes) for stack contents.\n"
            << "7. Use `symbol_from_address` on the crash address to identify the owning module/function.\n\n"
            << "## Phase 3: Root Cause Classification\n"
            << "Classify the crash as one of:\n"
            << "- NULL pointer dereference (pointer register is 0 or near-zero)\n"
            << "- Use-After-Free (pointer to freed memory, often 0xFEEEFEEE on debug heap)\n"
            << "- Buffer overflow (stack corruption, overwritten return address)\n"
            << "- Stack overflow (RSP/ESP outside stack region)\n"
            << "- DEP violation (executing non-executable memory)\n"
            << "- Integer overflow (bad calculation used as size/offset)\n"
            << "- Uninitialized memory (0xCCCCCCCC debug fill)\n"
            << "- Double free (crash inside RtlFreeHeap or similar)\n\n"
            << "## Output\n"
            << "Provide a structured report with: crash type, faulting instruction, root cause explanation, "
            << "evidence from registers/memory/stack, annotated call stack, and fix recommendations.";
    }
    else if (name == "find-vulnerability") {
        std::string vuln_type = args.value("vulnerability_type", "any");
        oss << "Perform a security vulnerability assessment. Focus type: " << vuln_type << "\n\n"
            << "## Phase 1: Reconnaissance\n"
            << "1. Use `module_get_main` and `module_list` to identify the target and dependencies.\n"
            << "2. Use `dump_analyze_module` to check security mitigations:\n"
            << "   - ASLR (DynamicBase), DEP/NX (NXCompat), Stack cookies (__security_check_cookie),\n"
            << "     SafeSEH, CFG (_guard_check_icall)\n"
            << "3. Use `symbol_list` on the main module to enumerate functions.\n\n"
            << "## Phase 2: Dangerous API Detection\n"
            << "Search for these dangerous APIs using `symbol_resolve` and `symbol_search`:\n"
            << "- Buffer overflow: strcpy, strcat, sprintf, gets, scanf, lstrcpyA/W\n"
            << "- Format string: printf, fprintf, sprintf, wprintf\n"
            << "- Memory corruption: memcpy, memmove with unchecked sizes\n"
            << "- Heap risks: HeapAlloc/HeapFree, malloc/free (double-free, UAF)\n"
            << "- Command injection: system, WinExec, ShellExecuteA/W, CreateProcessA/W\n\n"
            << "## Phase 3: Code Pattern Analysis\n"
            << "For each dangerous API found:\n"
            << "4. Use `disassembly_at` at the call site (count=30) to see how parameters are prepared.\n"
            << "5. Check for: size validation before copies, return value validation, "
            << "fixed-size stack buffers with unbounded operations.\n\n"
            << "## Phase 4: Input Entry Points\n"
            << "6. Identify input functions: ReadFile, recv, GetWindowTextA/W, RegQueryValueExA/W, "
            << "GetEnvironmentVariableA/W.\n"
            << "7. Trace data flow from input to dangerous APIs.\n\n"
            << "## Output\n"
            << "Provide: security mitigations checklist, findings ranked by severity (CRITICAL/HIGH/MEDIUM/LOW) "
            << "with CWE categories, evidence (disassembly), exploitation feasibility, and fix recommendations.";
    }
    else if (name == "trace-function") {
        std::string function_name = args.value("function_name", "unknown");
        oss << "Set up comprehensive tracing for function: " << function_name << "\n\n"
            << "## Phase 1: Locate the Function\n"
            << "1. Use `symbol_resolve` to resolve \"" << function_name << "\" to an address.\n"
            << "   If not found, use `symbol_search` with the name as a pattern.\n"
            << "2. Use `disassembly_function` at the resolved address for full disassembly.\n"
            << "3. Use `symbol_from_address` to get the fully qualified name.\n"
            << "4. Use `memory_get_info` to identify the owning module.\n\n"
            << "## Phase 2: Structural Analysis\n"
            << "5. From the disassembly, identify:\n"
            << "   - Function prologue and calling convention (x64 fastcall: RCX, RDX, R8, R9; x86: stack)\n"
            << "   - All CALL instructions (resolve each target with `symbol_from_address`)\n"
            << "   - Conditional branches and loops\n"
            << "   - Return points (all RET instructions)\n\n"
            << "## Phase 3: Set Up Tracing Breakpoints\n"
            << "6. Use `breakpoint_set` at function entry.\n"
            << "7. Use `breakpoint_set_log` with format:\n"
            << "   x64: \"ENTER " << function_name << " | RCX={RCX} RDX={RDX} R8={R8} R9={R9}\"\n"
            << "   x86: \"ENTER " << function_name << " | [ESP+4]={[ESP+4]:x} [ESP+8]={[ESP+8]:x}\"\n"
            << "8. For each RET instruction, set a logging breakpoint:\n"
            << "   \"EXIT " << function_name << " | RAX={RAX}\"\n"
            << "9. For key CALL sites inside the function, set additional logging breakpoints.\n\n"
            << "## Phase 4: Dynamic Capture\n"
            << "10. Use `context_get_snapshot` at entry to capture full state.\n"
            << "11. Step through with `debug_step_over`/`debug_step_into` for detailed tracing.\n"
            << "12. Use `context_get_snapshot` at exit and `context_compare_snapshots` to see changes.\n\n"
            << "## Output\n"
            << "Provide: function signature estimate, parameter descriptions, internal call graph, "
            << "control flow summary, return value type, and a list of all breakpoints set for monitoring.";
    }
    else if (name == "unpack-binary") {
        std::string packer_hint = args.value("packer_hint", "unknown");
        oss << "Unpack this packed/protected binary. Packer hint: " << packer_hint << "\n\n"
            << "## Phase 1: Packer Detection\n"
            << "1. Use `debug_get_state` to confirm the debugger is paused (ideally at entry point).\n"
            << "2. Use `module_get_main` to identify the target module.\n"
            << "3. Use `dump_analyze_module` to analyze:\n"
            << "   - Section names and characteristics (UPX0/UPX1, .themida, .vmp, .aspack = known packers)\n"
            << "   - Section entropy (>7.0 = likely packed/encrypted)\n"
            << "   - Entry point location relative to sections\n"
            << "   - Import count (very few imports = likely packed)\n"
            << "   - PE header anomalies\n"
            << "4. Use `disassembly_at` at the entry point (count=30) to examine the packer stub.\n\n"
            << "## Phase 2: OEP Detection\n"
            << "5. Use `dump_detect_oep` with the target module.\n"
            << "   Try strategies: hardware_bp, memory_bp, api_bp, section_bp.\n"
            << "6. If auto-detection fails, suggest manual approaches based on packer type:\n"
            << "   - UPX: POPAD + JMP sequence at end of stub\n"
            << "   - ASPack: breakpoint on VirtualProtect for section permission changes\n"
            << "   - Themida/VMProtect: monitor VirtualAlloc for executable memory allocation\n"
            << "   - General: hardware breakpoint on first section (write, then execute)\n\n"
            << "## Phase 3: Manual Dump at OEP\n"
            << "7. Once at OEP, use `dump_module` with the `oep` parameter set to the detected OEP:\n"
            << "   - module: target module name\n"
            << "   - output_path: [original_name]_unpacked.exe\n"
            << "   - oep: the OEP address detected in Phase 2\n"
            << "8. Use `dump_analyze_module` on the result to verify section reconstruction.\n\n"
            << "## Output\n"
            << "Provide: detected packer type, OEP address and detection method, "
            << "unpacking result (success/partial/failed), import recovery status, "
            << "output file path, and any post-processing applied.";
    }
    else if (name == "reverse-algorithm") {
        std::string start_address = args.value("start_address", "current");
        std::string descriptionText = args.value("description", "");
        oss << "Reverse engineer the algorithm at address: " << start_address << "\n";
        if (!descriptionText.empty()) {
            oss << "Description hint: " << descriptionText << "\n";
        }
        oss << "\n## Phase 1: Code Extraction\n"
            << "1. Use `disassembly_function` at the address for complete function disassembly.\n"
            << "   If function boundary detection fails, use `disassembly_at` with count=100.\n"
            << "2. Use `symbol_from_address` to check for a known function name.\n"
            << "3. Use `memory_get_info` to identify the owning module and section.\n\n"
            << "## Phase 2: Structural Analysis\n"
            << "4. Map the function structure: prologue, basic blocks, loops (backward jumps), "
            << "conditionals (forward jumps), switch tables.\n"
            << "5. Resolve all CALL targets with `symbol_from_address`.\n"
            << "6. For each referenced data address, use `memory_read` (64-256 bytes) to examine content.\n"
            << "7. Identify constants and magic numbers:\n"
            << "   - Crypto: 0x67452301 (MD5/SHA1), 0x6A09E667 (SHA-256), 0x61707865 (ChaCha20)\n"
            << "   - CRC: 0xEDB88320 (CRC32)\n"
            << "   - Check for S-boxes, lookup tables, round constants\n\n"
            << "## Phase 3: Dynamic Analysis (if debugger is paused at function entry)\n"
            << "8. Use `register_list` to capture input parameters.\n"
            << "9. Use `stack_read_frame` to read stack-passed parameters.\n"
            << "10. Step through one loop iteration with `debug_step_over` to observe values.\n\n"
            << "## Output\n"
            << "Provide: algorithm identification (type, known name if recognized, confidence), "
            << "estimated function signature, C-like pseudocode, data references (tables, constants), "
            << "control flow description, and notes on customizations or weaknesses.";
    }
    else if (name == "compare-execution") {
        oss << "Compare debugger state at two different execution points.\n\n"
            << "## Phase 1: Capture Snapshot A (Current State)\n"
            << "1. Use `debug_get_state` to confirm the debugger is paused.\n"
            << "2. Use `context_get_snapshot` with include_memory=true, include_stack=true.\n"
            << "3. Use `register_list` for detailed register values.\n"
            << "4. Use `disassembly_at` at current RIP/EIP (count=10) to record position.\n"
            << "5. Use `stack_get_trace` for the call stack.\n\n"
            << "Report Snapshot A captured, then wait for the user to advance execution.\n\n"
            << "## Phase 2: Capture Snapshot B (After Execution)\n"
            << "After the user advances execution (step, run_to, or continue to breakpoint):\n"
            << "6. Use `context_get_snapshot` with the same parameters.\n"
            << "7. Use `register_list` again.\n"
            << "8. Use `disassembly_at` at the new RIP/EIP (count=10).\n"
            << "9. Use `stack_get_trace` for the new call stack.\n\n"
            << "## Phase 3: Diff Analysis\n"
            << "10. Use `context_compare_snapshots` with both snapshots for the automated diff.\n"
            << "11. Additionally analyze: which registers changed and by how much, "
            << "CPU flag changes (ZF, CF, SF, OF), stack growth/shrink, "
            << "pushed/popped values, and call depth changes.\n\n"
            << "## Output\n"
            << "Provide a table of register changes (before/after/delta), flag changes, "
            << "stack changes, memory changes if captured, code positions at both snapshots, "
            << "call stack diff, and a natural language summary of what happened between the two points.";
    }
    else if (name == "hunt-strings") {
        std::string pattern = args.value("pattern", "");
        oss << "Search for interesting strings in the binary.\n";
        if (!pattern.empty()) {
            oss << "Search pattern: " << pattern << "\n";
        }
        oss << "\n## Phase 1: Context\n"
            << "1. Use `module_get_main` to identify the target.\n"
            << "2. Use `module_list` to get all loaded modules and address ranges.\n"
            << "3. Use `dump_analyze_module` to understand section layout (.rdata, .data, .rsrc).\n\n"
            << "## Phase 2: Search\n";
        if (!pattern.empty()) {
            oss << "4. Use `memory_search` with \"" << pattern << "\" as pattern in the main module range.\n"
                << "   Also search for UTF-16LE encoding (wide strings).\n"
                << "5. For each match, use `memory_read` at the address (size=256) for full string context.\n";
        } else {
            oss << "4. Search for high-value string categories using `memory_search`:\n"
                << "   - Credentials: password, secret, token, api_key, auth\n"
                << "   - Network: http://, https://, ftp://, .com, .net\n"
                << "   - File paths: C:\\, .exe, .dll, .dat, .cfg, .ini\n"
                << "   - Debug/Error: error, fail, debug, assert, exception\n"
                << "   - Registry: HKEY_, SOFTWARE\\, CurrentVersion\n"
                << "   - Crypto: AES, RSA, SHA, MD5, encrypt, decrypt\n";
        }
        oss << "\n## Phase 3: Cross-Reference Analysis\n"
            << "6. For each interesting string, note its address.\n"
            << "7. Search for references to the string address (little-endian) in code sections using `memory_search`.\n"
            << "8. For each cross-reference, use `disassembly_at` (count=15) to see how the string is used.\n"
            << "9. Use `symbol_from_address` to identify the referencing function.\n\n"
            << "## Output\n"
            << "Provide: findings organized by category (credentials, URLs, file paths, debug messages, etc.), "
            << "each with address, string content, referencing function, and usage context. "
            << "Highlight security-sensitive findings (hardcoded credentials, embedded URLs, crypto keys).";
    }
    else if (name == "patch-code") {
        std::string target = args.value("target_address", "");
        std::string goal = args.value("goal", "");
        oss << "Patch code at address " << target << " to achieve: " << goal << "\n\n"
            << "## Phase 1: Understand the Target\n"
            << "1. Use `disassembly_at` at " << target << " with count=30 for surrounding context.\n"
            << "2. Use `symbol_from_address` to identify the function and module.\n"
            << "3. Use `disassembly_function` for the complete containing function.\n\n"
            << "## Phase 2: Design the Patch\n"
            << "4. Design the minimal patch. Common patterns:\n"
            << "   - Bypass conditional: change JZ(74) to JMP(EB), or JZ to NOP NOP(9090)\n"
            << "   - NOP out a call: replace E8xxxxxxxx with 9090909090\n"
            << "   - Force return value: MOV EAX,1(B801000000)+RET(C3) or XOR EAX,EAX(33C0)+RET(C3)\n"
            << "5. Calculate exact bytes needed. Show before/after comparison.\n\n"
            << "## Phase 3: Apply (with user confirmation)\n"
            << "6. Use `memory_read` at target to backup original bytes.\n"
            << "7. After user confirms, use `memory_write` with the patch bytes (encoding=\"hex\").\n"
            << "8. Use `disassembly_at` to verify the patch was applied correctly.\n\n"
            << "## Phase 4: Verify\n"
            << "9. Confirm: disassembly correct, instruction alignment preserved, no side effects.\n"
            << "10. Suggest testing: set breakpoint before patch, run, step through.\n\n"
            << "## Output\n"
            << "Provide: original bytes (for restoration), patched bytes, before/after disassembly, "
            << "verification status, and the memory_write command to restore original bytes if needed. "
            << "Note: patch is in-memory only; use dump_module to persist.";
    }
    else if (name == "debug-session") {
        std::string issue = args.value("issue_description", "general debugging");
        oss << "Initialize a debugging session. Issue: " << issue << "\n\n"
            << "## Phase 1: Environment Assessment\n"
            << "1. Use `debug_get_state` to determine debugger state (running, paused, stopped).\n"
            << "2. Use `module_get_main` to identify the target binary.\n"
            << "3. Use `module_list` to enumerate all loaded modules.\n"
            << "4. Use `thread_list` to see all threads and their status.\n"
            << "5. Use `register_list` to capture the current register state.\n\n"
            << "## Phase 2: Initial Analysis\n"
            << "6. If paused at a valid address, use `disassembly_at` with current RIP/EIP (count=20).\n"
            << "7. Use `stack_get_trace` for the call stack.\n"
            << "8. Use `breakpoint_list` to check for existing breakpoints.\n\n"
            << "## Phase 3: Summary Report\n"
            << "Present a structured overview:\n"
            << "- Target binary name and path\n"
            << "- Debugger state and architecture (x86/x64)\n"
            << "- Module count with main module base address\n"
            << "- Thread count with current thread info\n"
            << "- Current disassembly position (5-10 instructions)\n"
            << "- Top 5 call stack frames with symbols\n"
            << "- Existing breakpoint count and list\n\n"
            << "## Phase 4: Recommendations\n"
            << "Based on the issue (\"" << issue << "\"), suggest:\n"
            << "- Where to set breakpoints for investigation\n"
            << "- Which modules or functions to examine\n"
            << "- Relevant API functions to monitor\n"
            << "- A step-by-step debugging strategy\n"
            << "If no specific issue is given, provide general recommendations based on binary type and loaded modules.";
    }
    else if (name == "api-monitor") {
        std::string api_category = args.value("api_category", "all");
        oss << "Set up API call monitoring. Category: " << api_category << "\n\n"
            << "## API Categories\n"
            << "Based on category \"" << api_category << "\", monitor these APIs:\n\n"
            << "**File**: CreateFileA/W, ReadFile, WriteFile, DeleteFileA/W, FindFirstFileA/W\n"
            << "**Network**: connect, send/recv, InternetOpenA/W, InternetConnectA/W, HttpSendRequestA/W\n"
            << "**Registry**: RegOpenKeyExA/W, RegQueryValueExA/W, RegSetValueExA/W, RegDeleteKeyA/W\n"
            << "**Crypto**: CryptEncrypt/Decrypt, BCryptEncrypt/Decrypt, BCryptHash\n"
            << "**Process**: CreateProcessA/W, CreateRemoteThread, VirtualAllocEx, WriteProcessMemory\n"
            << "**Memory**: VirtualAlloc, VirtualProtect, LoadLibraryA/W, GetProcAddress\n\n"
            << "If category is \"all\", monitor a representative set from each category.\n\n"
            << "## Setup Steps\n"
            << "For each target API:\n"
            << "1. Use `symbol_resolve` with the full name (e.g., \"kernel32.CreateFileW\") to get the address.\n"
            << "2. If resolved, use `breakpoint_set` at the address.\n"
            << "3. Use `breakpoint_set_log` with a descriptive format string that captures key parameters.\n"
            << "   x64 example: \"CreateFileW: path={[RCX]:us} access={RDX:x} share={R8:x}\"\n"
            << "   x86 example: \"CreateFileW: path={[[ESP+4]]:us} access={[ESP+8]:x}\"\n\n"
            << "## Output\n"
            << "Provide: a table of all monitored APIs with addresses and logging format, "
            << "total breakpoint count, instructions for running the target and reading log output, "
            << "and tips for filtering noise (e.g., adding conditions with breakpoint_set_condition). "
            << "Highlight suspicious patterns to watch for (e.g., CreateRemoteThread + WriteProcessMemory = injection).";
    }
    else {
        oss << "Unknown prompt template: " << name;
    }
    
    return oss.str();
}

json MCPPromptMessage::ToMCPFormat() const {
    return json{
        {"role", role},
        {"content", {
            {"type", "text"},
            {"text", content}
        }}
    };
}

json MCPPromptResult::ToMCPFormat() const {
    json messages_array = json::array();
    for (const auto& msg : messages) {
        messages_array.push_back(msg.ToMCPFormat());
    }
    
    return json{
        {"description", description},
        {"messages", messages_array}
    };
}

MCPPromptRegistry& MCPPromptRegistry::Instance() {
    static MCPPromptRegistry instance;
    return instance;
}

void MCPPromptRegistry::RegisterPrompt(const MCPPromptDefinition& prompt) {
    m_prompts.push_back(prompt);
}

void MCPPromptRegistry::RegisterDefaultPrompts() {
    Logger::Info("Registering default MCP prompts...");
    
    // 1. Crash Analysis
    RegisterPrompt({
        "analyze-crash",
        "Analyze Crash",
        "Comprehensive crash analysis with root cause identification",
        {
            {"crash_address", "Address where crash occurred (optional)", false}
        }
    });
    
    // 2. Vulnerability Hunting
    RegisterPrompt({
        "find-vulnerability",
        "Find Vulnerabilities",
        "Search for security vulnerabilities in the application",
        {
            {"vulnerability_type", "Type of vulnerability to focus on (e.g., buffer-overflow, use-after-free)", false}
        }
    });
    
    // 3. Function Tracing
    RegisterPrompt({
        "trace-function",
        "Trace Function Execution",
        "Set up comprehensive tracing for a specific function",
        {
            {"function_name", "Name or address of function to trace", true}
        }
    });
    
    // 4. Binary Unpacking
    RegisterPrompt({
        "unpack-binary",
        "Unpack Protected Binary",
        "Automated unpacking workflow for packed executables",
        {
            {"packer_hint", "Known packer type (e.g., UPX, Themida) or 'auto'", false}
        }
    });
    
    // 5. Algorithm Reverse Engineering
    RegisterPrompt({
        "reverse-algorithm",
        "Reverse Engineer Algorithm",
        "Analyze and document an algorithm's implementation",
        {
            {"start_address", "Starting address of the algorithm", true},
            {"description", "Brief description of expected algorithm behavior", false}
        }
    });
    
    // 6. Execution Comparison
    RegisterPrompt({
        "compare-execution",
        "Compare Execution States",
        "Compare debugger state at two different points",
        {}
    });
    
    // 7. String Hunting
    RegisterPrompt({
        "hunt-strings",
        "Hunt Interesting Strings",
        "Search for and analyze strings in the binary",
        {
            {"pattern", "Search pattern or keyword (optional)", false}
        }
    });
    
    // 8. Code Patching
    RegisterPrompt({
        "patch-code",
        "Patch Code",
        "Guided workflow for modifying executable code",
        {
            {"target_address", "Address to patch", true},
            {"goal", "What you want to achieve with the patch", true}
        }
    });
    
    // 9. Debug Session Start
    RegisterPrompt({
        "debug-session",
        "Start Debug Session",
        "Initialize a debugging session with context review",
        {
            {"issue_description", "Description of the issue to debug", false}
        }
    });
    
    // 10. API Monitoring
    RegisterPrompt({
        "api-monitor",
        "Monitor API Calls",
        "Set up monitoring for API function calls",
        {
            {"api_category", "Category of APIs (e.g., file, network, crypto)", false}
        }
    });
    
    Logger::Info("Registered {} prompts", m_prompts.size());
}

std::optional<MCPPromptDefinition> MCPPromptRegistry::FindPrompt(const std::string& name) const {
    for (const auto& prompt : m_prompts) {
        if (prompt.name == name) {
            return prompt;
        }
    }
    return std::nullopt;
}

json MCPPromptRegistry::GeneratePromptsListResponse() const {
    json prompts_array = json::array();
    
    for (const auto& prompt : m_prompts) {
        prompts_array.push_back(prompt.ToMCPFormat());
    }
    
    return json{
        {"prompts", prompts_array}
    };
}

MCPPromptResult MCPPromptRegistry::GetPrompt(const std::string& name, const json& args) const {
    auto promptOpt = FindPrompt(name);
    if (!promptOpt.has_value()) {
        throw std::runtime_error("Prompt not found: " + name);
    }
    
    const MCPPromptDefinition& prompt = promptOpt.value();
    
    // Validate required arguments
    for (const auto& arg : prompt.arguments) {
        if (arg.required && (!args.contains(arg.name) || args[arg.name].is_null())) {
            throw std::runtime_error("Missing required argument: " + arg.name);
        }
    }
    
    // Generate prompt content
    std::string promptText = prompt.GeneratePrompt(args);
    
    MCPPromptResult result;
    result.description = prompt.description;
    result.messages = {
        {"user", promptText}
    };
    
    return result;
}

} // namespace MCP
