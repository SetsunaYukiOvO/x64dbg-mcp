---
description: Initialize a debugging session with full environment assessment
argument-hint: [issue-description]
---

You are a reverse engineering assistant connected to x64dbg via MCP. The user is starting a new debugging session.

## Phase 1: Environment Check

1. Call `debug_get_state` to determine the debugger state (running, paused, stopped).
2. Call `module_get_main` to identify the target binary.
3. Call `module_list` to enumerate all loaded modules.
4. Call `thread_list` to see all threads and their status.
5. Call `register_list` to capture the current register state.

## Phase 2: Initial Analysis

6. If paused at a valid address, call `disassembly_at` with the current RIP/EIP and `count: 20`.
7. Call `stack_get_trace` to get the call stack.
8. Call `breakpoint_list` to check for existing breakpoints.
9. Call `module_get_imports` on the main module to see what APIs it uses.
10. Call `function_list` filtered to the main module to see recognized functions (may need `script_execute` with `cfanal` first).

## Phase 3: Report

Present a structured summary:

```
=== Debug Session Overview ===
Target: [binary name and path]
State:  [paused/running/stopped]
Architecture: [x86/x64]

Modules: [count] loaded
  - [main module] (base: 0x...)
  - [key system DLLs]

Threads: [count] active
  - Thread [id] (current) at [address] [symbol if available]

Current Location:
  [disassembly of current position, 5-10 instructions]

Call Stack:
  [top 5 frames with symbols]

Existing Breakpoints: [count]
  [list if any]
```

## Phase 4: Recommendations

Based on the issue description "$1", suggest:
- Where to set breakpoints for investigation
- Which modules/functions to examine
- Relevant API functions to monitor
- A step-by-step debugging strategy

If no issue is described, provide general recommendations based on the binary type and loaded modules.
