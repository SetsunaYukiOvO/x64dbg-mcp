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
        oss << "I need help analyzing a crash in my application.\n\n"
            << "Context:\n"
            << "- Crash address: " << crash_address << "\n"
            << "- Please examine the crash location, registers, and stack trace\n"
            << "- Identify the root cause of the crash\n"
            << "- Suggest potential fixes\n\n"
            << "Use available resources (debugger://state/current, debugger://registers/all, debugger://stack/trace) "
            << "and tools (disassembly_at, memory_read) to investigate.";
    }
    else if (name == "find-vulnerability") {
        std::string vuln_type = args.value("vulnerability_type", "any");
        oss << "Help me search for potential security vulnerabilities in the loaded application.\n\n"
            << "Focus areas:\n"
            << "- Type: " << vuln_type << "\n"
            << "- Check for buffer overflows, use-after-free, format string bugs\n"
            << "- Analyze dangerous API calls and memory operations\n"
            << "- Review input validation and boundary checks\n\n"
            << "Use disassembly and memory analysis tools to investigate suspicious code patterns.";
    }
    else if (name == "trace-function") {
        std::string function_name = args.value("function_name", "unknown");
        oss << "I want to trace the execution and behavior of a specific function.\n\n"
            << "Target: " << function_name << "\n\n"
            << "Please:\n"
            << "1. Set breakpoints at function entry and key points\n"
            << "2. Monitor register values and parameter passing\n"
            << "3. Track memory accesses and modifications\n"
            << "4. Document the execution flow\n\n"
            << "Use breakpoint_set, register_get, and memory_read tools as needed.";
    }
    else if (name == "unpack-binary") {
        std::string packer_hint = args.value("packer_hint", "unknown");
        oss << "Help me unpack this packed/protected binary.\n\n"
            << "Packer hint: " << packer_hint << "\n\n"
            << "Steps:\n"
            << "1. Analyze the binary to detect the packer type\n"
            << "2. Locate the Original Entry Point (OEP)\n"
            << "3. Dump the unpacked code from memory\n"
            << "4. Rebuild the PE structure\n\n"
            << "Use dump_analyze_module, dump_detect_oep, and dump_auto_unpack tools.";
    }
    else if (name == "reverse-algorithm") {
        std::string start_address = args.value("start_address", "current");
        std::string descriptionText = args.value("description", "");
        oss << "I need help reverse engineering an algorithm.\n\n"
            << "Location: " << start_address << "\n";
        if (!descriptionText.empty()) {
            oss << "Description: " << descriptionText << "\n";
        }
        oss << "\nPlease:\n"
            << "1. Disassemble and analyze the code\n"
            << "2. Identify the algorithm's purpose and logic\n"
            << "3. Document key operations and data transformations\n"
            << "4. Provide a high-level explanation or pseudocode\n\n"
            << "Use disassembly_function, memory_read, and register analysis.";
    }
    else if (name == "compare-execution") {
        oss << "Help me compare two different execution paths or states.\n\n"
            << "Please:\n"
            << "1. Capture the initial state (registers, memory)\n"
            << "2. Execute to the comparison point\n"
            << "3. Capture the final state\n"
            << "4. Highlight all differences\n\n"
            << "Use context_get_snapshot and context_compare_snapshots tools.";
    }
    else if (name == "hunt-strings") {
        std::string pattern = args.value("pattern", "");
        oss << "Search for interesting strings and references in the binary.\n\n";
        if (!pattern.empty()) {
            oss << "Pattern: " << pattern << "\n";
        }
        oss << "\nPlease:\n"
            << "1. Search memory for strings matching the pattern\n"
            << "2. Find cross-references to those strings\n"
            << "3. Analyze how the strings are used\n"
            << "4. Identify potential hardcoded credentials, API keys, or debug info\n\n"
            << "Use memory_search and symbol resolution tools.";
    }
    else if (name == "patch-code") {
        std::string target = args.value("target_address", "");
        std::string goal = args.value("goal", "");
        oss << "Help me patch the code to modify its behavior.\n\n"
            << "Target: " << target << "\n"
            << "Goal: " << goal << "\n\n"
            << "Please:\n"
            << "1. Analyze the current code at the target location\n"
            << "2. Suggest appropriate assembly instructions for the patch\n"
            << "3. Explain the impact of the changes\n"
            << "4. Apply the patch if I confirm\n\n"
            << "Use disassembly and memory_write tools carefully.";
    }
    else if (name == "debug-session") {
        std::string issue = args.value("issue_description", "general debugging");
        oss << "I'm starting a debugging session.\n\n"
            << "Issue: " << issue << "\n\n"
            << "Please:\n"
            << "1. Review the current debugger state\n"
            << "2. Examine loaded modules and threads\n"
            << "3. Check for any obvious issues (crashes, exceptions, suspicious values)\n"
            << "4. Suggest next steps for investigation\n\n"
            << "Use debugger://state/current and debugger://modules/list resources.";
    }
    else if (name == "api-monitor") {
        std::string api_category = args.value("api_category", "all");
        oss << "Monitor API calls made by the application.\n\n"
            << "Category: " << api_category << "\n\n"
            << "Please:\n"
            << "1. Identify key API functions to monitor\n"
            << "2. Set breakpoints on those APIs\n"
            << "3. Log parameters and return values\n"
            << "4. Analyze the API usage patterns\n\n"
            << "Use breakpoint_set with logging and symbol resolution tools.";
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
