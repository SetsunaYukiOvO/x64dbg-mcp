#pragma once

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class ContextHandler {
public:
    // Get complete debugging context snapshot
    static json getSnapshot(const json& params);
    
    // Get simplified context (registers + basic state)
    static json getBasicContext(const json& params);
    
    // Compare two snapshots
    static json compareSnapshots(const json& params);

private:
    // Helper functions
    static json captureRegisters();
    static json captureBreakpoints();
    static json captureThreads();
    static json captureStack();
    static json captureModules();
};
