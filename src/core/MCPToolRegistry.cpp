#include "MCPToolRegistry.h"
#include "Logger.h"
#include <sstream>

namespace MCP {
namespace {

std::string BuildFallbackToolDescription(const MCPToolDefinition& tool) {
    if (!tool.jsonrpcMethod.empty()) {
        return "Invoke " + tool.jsonrpcMethod + " operation.";
    }

    return "Invoke " + tool.name + ".";
}

std::string BuildFallbackParameterDescription(const MCPToolParameter& parameter) {
    return "Input parameter '" + parameter.name + "'.";
}

} // namespace

json MCPToolParameter::ToSchema() const {
    json schema;
    schema["type"] = type;
    schema["description"] = description.empty() ? BuildFallbackParameterDescription(*this) : description;
    
    if (!defaultValue.is_null()) {
        schema["default"] = defaultValue;
    }
    
    if (enumValues.is_array() && !enumValues.empty()) {
        schema["enum"] = enumValues;
    }
    
    // Array 类型必须定义 items
    if (type == "array" && !arrayItemsSchema.is_null()) {
        schema["items"] = arrayItemsSchema;
    }
    
    return schema;
}

json MCPToolDefinition::ToMCPFormat() const {
    json inputSchema;
    inputSchema["type"] = "object";
    inputSchema["properties"] = json::object();
    json required_fields = json::array();
    
    for (const auto& param : parameters) {
        inputSchema["properties"][param.name] = param.ToSchema();
        if (param.required) {
            required_fields.push_back(param.name);
        }
    }
    
    if (!required_fields.empty()) {
        inputSchema["required"] = required_fields;
    }
    
    return {
        {"name", name},
        {"description", description.empty() ? BuildFallbackToolDescription(*this) : description},
        {"inputSchema", inputSchema}
    };
}

std::string MCPToolDefinition::ValidateArguments(const json& args) const {
    if (!args.is_object()) {
        return "Arguments must be an object";
    }
    
    // 检查必需参数
    for (const auto& param : parameters) {
        if (param.required && !args.contains(param.name)) {
            return "Missing required parameter: " + param.name;
        }
        
        if (args.contains(param.name)) {
            const json& value = args[param.name];
            
            // 类型检查
            if (param.type == "string" && !value.is_string()) {
                return "Parameter '" + param.name + "' must be a string";
            }
            else if (param.type == "number" && !value.is_number()) {
                return "Parameter '" + param.name + "' must be a number";
            }
            else if (param.type == "integer" && !value.is_number_integer()) {
                return "Parameter '" + param.name + "' must be an integer";
            }
            else if (param.type == "boolean" && !value.is_boolean()) {
                return "Parameter '" + param.name + "' must be a boolean";
            }
            else if (param.type == "object" && !value.is_object()) {
                return "Parameter '" + param.name + "' must be an object";
            }
            else if (param.type == "array" && !value.is_array()) {
                return "Parameter '" + param.name + "' must be an array";
            }
            
            // 枚举值检查
            if (param.enumValues.is_array() && !param.enumValues.empty()) {
                bool found = false;
                for (const auto& enumValue : param.enumValues) {
                    if (value == enumValue) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return "Parameter '" + param.name + "' must be one of the allowed values";
                }
            }
        }
    }
    
    return "";  // 验证通过
}

json MCPToolDefinition::TransformToJSONRPC(const json& mcpArgs) const {
    // MCP arguments 通常已经是正确的格式,直接返回
    return mcpArgs;
}

MCPToolRegistry& MCPToolRegistry::Instance() {
    static MCPToolRegistry instance;
    return instance;
}

void MCPToolRegistry::RegisterTool(const MCPToolDefinition& tool) {
    m_tools[tool.name] = tool;
    Logger::Debug("Registered MCP tool: {}", tool.name);
}

std::vector<MCPToolDefinition> MCPToolRegistry::GetAllTools() const {
    std::vector<MCPToolDefinition> tools;
    tools.reserve(m_tools.size());
    
    for (const auto& pair : m_tools) {
        tools.push_back(pair.second);
    }
    
    return tools;
}

std::optional<MCPToolDefinition> MCPToolRegistry::FindTool(const std::string& name) const {
    auto it = m_tools.find(name);
    if (it != m_tools.end()) {
        return it->second;
    }
    return std::nullopt;
}

json MCPToolRegistry::GenerateToolsListResponse() const {
    json tools_array = json::array();
    
    for (const auto& pair : m_tools) {
        tools_array.push_back(pair.second.ToMCPFormat());
    }
    
    return {
        {"tools", tools_array}
    };
}

void MCPToolRegistry::RegisterDefaultTools() {
    Logger::Info("Registering default MCP tools...");
    
    // 1. Debug Control Tools
    RegisterTool({
        "debug_get_state",
        "Get current debugger state (stopped/running/paused) and instruction pointer",
        "debug.get_state",
        {}
    });
    
    RegisterTool({
        "debug_run",
        "Continue execution until next breakpoint or exception",
        "debug.run",
        {}
    });
    
    RegisterTool({
        "debug_pause",
        "Pause program execution",
        "debug.pause",
        {}
    });
    
    RegisterTool({
        "debug_step_into",
        "Execute single instruction (step into calls)",
        "debug.step_into",
        {}
    });
    
    RegisterTool({
        "debug_step_over",
        "Execute single instruction (step over calls)",
        "debug.step_over",
        {}
    });
    
    RegisterTool({
        "debug_step_out",
        "Execute until return from current function",
        "debug.step_out",
        {}
    });
    
    RegisterTool({
        "debug_run_to",
        "Run until reaching specified address",
        "debug.run_to",
        {
            {"address", "string", "Target address (hex format, e.g. '0x401000')", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "debug_restart",
        "Restart debugging session",
        "debug.restart",
        {}
    });
    
    RegisterTool({
        "debug_stop",
        "Stop debugging and close target process",
        "debug.stop",
        {}
    });
    
    // 2. Register Tools
    RegisterTool({
        "register_get",
        "Read value of a single register",
        "register.get",
        {
            {"name", "string", "Register name (e.g. 'rax', 'rip', 'eflags')", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "register_set",
        "Write value to a register",
        "register.set",
        {
            {"name", "string", "Register name", true, nullptr, nullptr},
            {"value", "string", "New value (hex format)", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "register_list",
        "List all registers with their current values",
        "register.list",
        {
            {"general_only", "boolean", "Only return general purpose registers", false, false, nullptr}
        }
    });
    
    RegisterTool({
        "register_get_batch",
        "Read multiple registers at once",
        "register.get_batch",
        {
            {"names", "array", "Array of register names", true, nullptr, nullptr, 
             json{{"type", "string"}}}
        }
    });
    
    // 3. Memory Tools
    RegisterTool({
        "memory_read",
        "Read memory from specified address",
        "memory.read",
        {
            {"address", "string", "Memory address (hex format)", true, nullptr, nullptr},
            {"size", "integer", "Number of bytes to read", true, nullptr, nullptr},
            {"encoding", "string", "Output encoding", false, "hex", json::array({"hex", "ascii", "base64"})}
        }
    });
    
    RegisterTool({
        "memory_write",
        "Write data to memory",
        "memory.write",
        {
            {"address", "string", "Target address (hex format)", true, nullptr, nullptr},
            {"data", "string", "Data to write", true, nullptr, nullptr},
            {"encoding", "string", "Data encoding", false, "hex", json::array({"hex", "ascii", "base64"})}
        }
    });
    
    RegisterTool({
        "memory_search",
        "Search for byte pattern in memory",
        "memory.search",
        {
            {"pattern", "string", "Search pattern (hex with ?? wildcards, e.g. '48 8B ?? C3')", true, nullptr, nullptr},
            {"start", "string", "Start address (optional)", false, nullptr, nullptr},
            {"end", "string", "End address (optional)", false, nullptr, nullptr},
            {"max_results", "integer", "Maximum results to return", false, 1000, nullptr}
        }
    });
    
    RegisterTool({
        "memory_get_info",
        "Get information about memory region at address",
        "memory.get_info",
        {
            {"address", "string", "Address to query", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "memory_enumerate",
        "List all memory regions in the process",
        "memory.enumerate",
        {}
    });
    
    RegisterTool({
        "memory_allocate",
        "Allocate new memory in target process",
        "memory.allocate",
        {
            {"size", "integer", "Size in bytes to allocate", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "memory_free",
        "Free previously allocated memory",
        "memory.free",
        {
            {"address", "string", "Address of memory to free", true, nullptr, nullptr}
        }
    });
    
    // 4. Breakpoint Tools
    RegisterTool({
        "breakpoint_set",
        "Set a breakpoint at specified address",
        "breakpoint.set",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr},
            {"type", "string", "Breakpoint type", false, "software", 
             json::array({"software", "hardware", "memory"})},
            {"enabled", "boolean", "Enable immediately", false, true, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_delete",
        "Remove a breakpoint",
        "breakpoint.delete",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_enable",
        "Enable a disabled breakpoint",
        "breakpoint.enable",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_disable",
        "Disable a breakpoint without removing it",
        "breakpoint.disable",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_list",
        "List all breakpoints",
        "breakpoint.list",
        {}
    });
    
    // 5. Disassembly Tools
    RegisterTool({
        "disassembly_at",
        "Disassemble instructions at address",
        "disassembly.at",
        {
            {"address", "string", "Starting address", true, nullptr, nullptr},
            {"count", "integer", "Number of instructions", false, 10, nullptr}
        }
    });
    
    RegisterTool({
        "disassembly_function",
        "Disassemble entire function",
        "disassembly.function",
        {
            {"address", "string", "Address within function", true, nullptr, nullptr}
        }
    });
    
    // 6. Symbol Tools
    RegisterTool({
        "symbol_resolve",
        "Resolve symbol name to address",
        "symbol.resolve",
        {
            {"symbol", "string", "Symbol name (e.g. 'kernel32.GetProcAddress')", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "symbol_from_address",
        "Get symbol name from address",
        "symbol.from_address",
        {
            {"address", "string", "Address to lookup", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "symbol_search",
        "Search symbols by pattern",
        "symbol.search",
        {
            {"pattern", "string", "Search pattern (supports wildcards)", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "module_list",
        "List all loaded modules",
        "module.list",
        {}
    });
    
    // 7. Thread Tools
    RegisterTool({
        "thread_list",
        "List all threads in debugged process",
        "thread.list",
        {}
    });
    
    RegisterTool({
        "thread_get_current",
        "Get current thread information",
        "thread.get_current",
        {}
    });
    
    RegisterTool({
        "thread_switch",
        "Switch to different thread",
        "thread.switch",
        {
            {"thread_id", "integer", "Thread ID to switch to", true, nullptr, nullptr}
        }
    });
    
    // 8. Stack Tools
    RegisterTool({
        "stack_get_trace",
        "Get stack trace (call stack)",
        "stack.get_trace",
        {}
    });
    
    RegisterTool({
        "stack_read_frame",
        "Read stack frame data at specified address",
        "stack.read_frame",
        {
            {"address", "string", "Stack frame address (hex format)", true, nullptr, nullptr},
            {"size", "integer", "Number of bytes to read (max 64KB)", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "stack_get_pointers",
        "Get stack pointer values (RSP, RBP)",
        "stack.get_pointers",
        {}
    });
    
    RegisterTool({
        "stack_is_on_stack",
        "Check if address is on the stack",
        "stack.is_on_stack",
        {
            {"address", "string", "Address to check", true, nullptr, nullptr}
        }
    });
    
    // 9. Extended Breakpoint Tools
    RegisterTool({
        "breakpoint_get",
        "Get detailed information about a breakpoint",
        "breakpoint.get",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_delete_all",
        "Delete all breakpoints",
        "breakpoint.delete_all",
        {}
    });
    
    RegisterTool({
        "breakpoint_toggle",
        "Toggle breakpoint state (enabled/disabled)",
        "breakpoint.toggle",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_set_condition",
        "Set breakpoint condition expression",
        "breakpoint.set_condition",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr},
            {"condition", "string", "Condition expression (e.g. 'rax==5')", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_set_log",
        "Set breakpoint log message",
        "breakpoint.set_log",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr},
            {"log_text", "string", "Log message to print when breakpoint is hit", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "breakpoint_reset_hitcount",
        "Reset breakpoint hit counter",
        "breakpoint.reset_hitcount",
        {
            {"address", "string", "Breakpoint address", true, nullptr, nullptr}
        }
    });
    
    // 10. Extended Symbol Tools
    RegisterTool({
        "symbol_list",
        "List symbols in a module",
        "symbol.list",
        {
            {"module", "string", "Module name (optional, defaults to main module)", false, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "symbol_set_label",
        "Set user-defined label at address",
        "symbol.set_label",
        {
            {"address", "string", "Address for label", true, nullptr, nullptr},
            {"label", "string", "Label text", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "symbol_set_comment",
        "Set comment at address",
        "symbol.set_comment",
        {
            {"address", "string", "Address for comment", true, nullptr, nullptr},
            {"comment", "string", "Comment text", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "symbol_get_comment",
        "Get comment at address",
        "symbol.get_comment",
        {
            {"address", "string", "Address to query", true, nullptr, nullptr}
        }
    });
    
    // 11. Extended Module Tools
    RegisterTool({
        "module_get",
        "Get detailed module information",
        "module.get",
        {
            {"module", "string", "Module name or base address", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "module_get_main",
        "Get main executable module information",
        "module.get_main",
        {}
    });
    
    // 12. Extended Thread Tools
    RegisterTool({
        "thread_get",
        "Get detailed thread information",
        "thread.get",
        {
            {"thread_id", "integer", "Thread ID", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "thread_suspend",
        "Suspend a thread",
        "thread.suspend",
        {
            {"thread_id", "integer", "Thread ID to suspend", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "thread_resume",
        "Resume a suspended thread",
        "thread.resume",
        {
            {"thread_id", "integer", "Thread ID to resume", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "thread_get_count",
        "Get total number of threads",
        "thread.get_count",
        {}
    });
    
    // 13. Extended Disassembly Tools
    RegisterTool({
        "disassembly_range",
        "Disassemble a range of addresses",
        "disassembly.range",
        {
            {"start", "string", "Start address", true, nullptr, nullptr},
            {"end", "string", "End address", true, nullptr, nullptr},
            {"max_instructions", "integer", "Maximum instructions to disassemble", false, 10000, nullptr}
        }
    });
    
    // 14. Dump & Unpacking Tools
    RegisterTool({
        "dump_module",
        "Dump module to file with PE header reconstruction and optional OEP override",
        "dump.module",
        {
            {"module", "string", "Module name or base address", true, nullptr, nullptr},
            {"output_path", "string", "Output file path", true, nullptr, nullptr},
            {"oep", "string", "Force this address as the new entry point (hex)", false, nullptr, nullptr},
            {"rebuild_pe", "boolean", "Rebuild PE headers and align sections", false, true, nullptr},
            {"remove_integrity_check", "boolean", "Clear PE checksum", false, true, nullptr},
            {"auto_detect_oep", "boolean", "Try to auto-detect OEP via pattern analysis", false, false, nullptr},
            {"dump_full_image", "boolean", "Dump full image including non-committed pages", false, false, nullptr}
        }
    });
    
    RegisterTool({
        "dump_memory_region",
        "Dump arbitrary memory region to file",
        "dump.memory_region",
        {
            {"address", "string", "Start address", true, nullptr, nullptr},
            {"size", "integer", "Size in bytes", true, nullptr, nullptr},
            {"output_path", "string", "Output file path", true, nullptr, nullptr},
            {"as_raw_binary", "boolean", "Store bytes as-is without PE rebuild", false, false, nullptr}
        }
    });

    RegisterTool({
        "dump_analyze_module",
        "Analyze module and detect packer",
        "dump.analyze_module",
        {
            {"module", "string", "Module name or base address (optional, defaults to main module)", false, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "dump_detect_oep",
        "Detect Original Entry Point (OEP) by scanning for unpacker stub transfer patterns",
        "dump.detect_oep",
        {
            {"module", "string", "Module name or base address", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "dump_get_dumpable_regions",
        "List dumpable memory regions",
        "dump.get_dumpable_regions",
        {
            {"module_base", "string", "Optional module base address filter", false, nullptr, nullptr}
        }
    });
    
    // 15. Script Execution Tools
    RegisterTool({
        "script_execute",
        "Execute single x64dbg script command",
        "script.execute",
        {
            {"command", "string", "x64dbg command to execute (e.g. 'bp 401000')", true, nullptr, nullptr}
        }
    });
    
    RegisterTool({
        "script_execute_batch",
        "Execute multiple x64dbg commands in sequence",
        "script.execute_batch",
        {
            {"commands", "array", "Array of x64dbg commands", true, nullptr, nullptr,
             json{{"type", "string"}}},
            {"stop_on_error", "boolean", "Stop execution if a command fails", false, true, nullptr}
        }
    });
    
    RegisterTool({
        "script_get_last_result",
        "Get result of last script execution",
        "script.get_last_result",
        {}
    });
    
    // 16. Context Snapshot Tools
    RegisterTool({
        "context_get_snapshot",
        "Capture complete debugging context snapshot (registers, memory, stack, etc.)",
        "context.get_snapshot",
        {
            {"include_memory", "boolean", "Include memory dump in snapshot", false, false, nullptr},
            {"include_stack", "boolean", "Include stack data in snapshot", false, true, nullptr}
        }
    });
    
    RegisterTool({
        "context_get_basic",
        "Get basic context (registers and debugger state only)",
        "context.get_basic",
        {}
    });
    
    RegisterTool({
        "context_compare_snapshots",
        "Compare two context snapshots and show differences",
        "context.compare_snapshots",
        {
            {"snapshot1", "object", "First snapshot (from context.get_snapshot)", true, nullptr, nullptr},
            {"snapshot2", "object", "Second snapshot (from context.get_snapshot)", true, nullptr, nullptr}
        }
    });

    // 17. Expression Evaluation
    RegisterTool({
        "eval_expression",
        "Evaluate an x64dbg expression (math, symbols, registers, memory dereferences like [rsp+8])",
        "eval.expression",
        {
            {"expression", "string", "Expression to evaluate (e.g. 'rax+rbx*2', '[rsp+8]', 'kernel32.base')", true, nullptr, nullptr}
        }
    });

    // 18. Cross-Reference Analysis
    RegisterTool({
        "xref_get",
        "Get cross-references (callers, jumps, data refs) to an address",
        "xref.get",
        {
            {"address", "string", "Target address to find references to", true, nullptr, nullptr}
        }
    });

    // 19. Function Analysis
    RegisterTool({
        "function_list",
        "List all recognized functions in the debuggee",
        "function.list",
        {
            {"module", "string", "Filter by module name (optional)", false, nullptr, nullptr}
        }
    });

    RegisterTool({
        "function_get",
        "Get function boundaries (start, end, size) at an address",
        "function.get",
        {
            {"address", "string", "Address within the function", true, nullptr, nullptr}
        }
    });

    // 20. Module Imports/Exports
    RegisterTool({
        "module_get_exports",
        "List exported functions of a module",
        "module.get_exports",
        {
            {"module", "string", "Module name or base address", true, nullptr, nullptr}
        }
    });

    RegisterTool({
        "module_get_imports",
        "List imported functions of a module",
        "module.get_imports",
        {
            {"module", "string", "Module name or base address", true, nullptr, nullptr}
        }
    });

    // 21. Assembler
    RegisterTool({
        "assembler_assemble",
        "Assemble an instruction to machine code bytes, optionally writing to memory",
        "assembler.assemble",
        {
            {"instruction", "string", "Assembly instruction (e.g. 'nop', 'jmp 0x401000', 'mov eax, 1')", true, nullptr, nullptr},
            {"address", "string", "Address context for assembly (affects relative offsets)", true, nullptr, nullptr},
            {"write_to_memory", "boolean", "Write assembled bytes directly to target memory", false, false, nullptr}
        }
    });

    // 22. Bookmarks
    RegisterTool({
        "bookmark_set",
        "Set a bookmark at an address",
        "bookmark.set",
        {
            {"address", "string", "Address to bookmark", true, nullptr, nullptr}
        }
    });

    RegisterTool({
        "bookmark_delete",
        "Delete a bookmark at an address",
        "bookmark.delete",
        {
            {"address", "string", "Address to remove bookmark from", true, nullptr, nullptr}
        }
    });

    RegisterTool({
        "bookmark_list",
        "List all bookmarks",
        "bookmark.list",
        {}
    });

    // 23. Patch Management
    RegisterTool({
        "patch_list",
        "List all applied patches (byte-level modifications)",
        "patch.list",
        {}
    });

    RegisterTool({
        "patch_restore",
        "Restore original bytes at a patched address",
        "patch.restore",
        {
            {"address", "string", "Address to restore original bytes", true, nullptr, nullptr}
        }
    });

    Logger::Info("Registered {} MCP tools", m_tools.size());
}

} // namespace MCP
