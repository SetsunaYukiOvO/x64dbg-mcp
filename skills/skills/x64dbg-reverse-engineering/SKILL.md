---
name: x64dbg Reverse Engineering
description: This skill should be used when performing binary analysis, debugging, reverse engineering, unpacking, or vulnerability research with x64dbg debugger via MCP. Provides expert knowledge on x64dbg MCP tools, Windows internals, assembly patterns, and reverse engineering methodology.
version: 1.0.0
---

# x64dbg Reverse Engineering Skill

Expert knowledge base for reverse engineering with x64dbg debugger through MCP protocol.

## When to Use

Activate this skill when:
- Analyzing binary executables on Windows
- Debugging applications with x64dbg/x32dbg
- Unpacking protected binaries
- Hunting for vulnerabilities
- Reverse engineering algorithms or protocols
- Monitoring API calls and system behavior
- Patching binary code

## Available MCP Tools (79 total)

### Debug Control (10 tools)
- `debug_get_state` - Check debugger state (paused/running/stopped)
- `debug_init` - Start a new debug session by loading an executable (params: `path?`, `arguments?`, `current_dir?`)
- `debug_run` - Continue execution
- `debug_pause` - Break execution
- `debug_step_into` - Single step into calls
- `debug_step_over` - Single step over calls
- `debug_step_out` - Execute until function returns
- `debug_run_to` - Run to specific address (param: `address`)
- `debug_restart` - Restart the debug session
- `debug_stop` - Stop debugging

### Registers (4 tools)
- `register_get` - Read one register (param: `name`)
- `register_set` - Write register (params: `name`, `value`)
- `register_list` - List all registers (optional: `general_only`)
- `register_get_batch` - Read multiple registers (param: `names` array)

### Memory (7 tools)
- `memory_read` - Read memory (params: `address`, `size`; optional: `encoding`)
- `memory_write` - Write memory (params: `address`, `data`; optional: `encoding`)
- `memory_search` - Search pattern (param: `pattern`; optional: `start`, `end`, `max_results`)
- `memory_get_info` - Region info (param: `address`)
- `memory_enumerate` - List all regions
- `memory_allocate` - Allocate memory (param: `size`)
- `memory_free` - Free memory (param: `address`)

### Breakpoints (11 tools)
- `breakpoint_set` - Set breakpoint (param: `address`; optional: `type`, `enabled`)
- `breakpoint_delete` - Remove breakpoint (param: `address`)
- `breakpoint_enable` / `breakpoint_disable` / `breakpoint_toggle`
- `breakpoint_list` - List all breakpoints
- `breakpoint_get` - Get details (param: `address`)
- `breakpoint_delete_all` - Remove all
- `breakpoint_set_condition` - Set condition (params: `address`, `condition`)
- `breakpoint_set_log` - Set log message (params: `address`, `log_text`)
- `breakpoint_reset_hitcount` - Reset counter (param: `address`)

### Disassembly (3 tools)
- `disassembly_at` - Disassemble at address (params: `address`, `count`)
- `disassembly_function` - Disassemble entire function (param: `address`)
- `disassembly_range` - Disassemble range (params: `start`, `end`)

### Symbols (7 tools)
- `symbol_resolve` - Name to address (param: `symbol`)
- `symbol_from_address` - Address to name (param: `address`)
- `symbol_search` - Search symbols (param: `pattern`)
- `symbol_list` - List symbols (optional: `module`)
- `symbol_set_label` - Set label (params: `address`, `label`)
- `symbol_set_comment` / `symbol_get_comment`

### Modules (5 tools)
- `module_list` - All loaded modules
- `module_get` - Module info (param: `module`)
- `module_get_main` - Main executable module
- `module_get_exports` - List module exports (param: `module`)
- `module_get_imports` - List module imports (param: `module`)

### Threads (7 tools)
- `thread_list` - All threads
- `thread_get_current` - Current thread
- `thread_switch` - Switch thread (param: `thread_id`)
- `thread_get` - Thread info (param: `thread_id`)
- `thread_suspend` / `thread_resume` / `thread_get_count`

### Stack (4 tools)
- `stack_get_trace` - Call stack trace
- `stack_read_frame` - Read stack frame (params: `address`, `size`)
- `stack_get_pointers` - RSP/RBP values
- `stack_is_on_stack` - Check address (param: `address`)

### Dump (5 tools)
- `dump_module` - Dump module to file with PE rebuild and optional OEP override
- `dump_memory_region` - Dump raw memory region
- `dump_analyze_module` - PE analysis with entropy and packer detection
- `dump_detect_oep` - Detect Original Entry Point via pattern analysis
- `dump_get_dumpable_regions` - List dumpable regions

### Script (3 tools)
- `script_execute` - Run x64dbg command (param: `command`)
- `script_execute_batch` - Run batch commands (param: `commands` array)
- `script_get_last_result` - Get last result

### Context Snapshots (3 tools)
- `context_get_snapshot` - Full state capture
- `context_get_basic` - Quick register + state check
- `context_compare_snapshots` - Diff two snapshots

### Expression Evaluation (1 tool)
- `eval_expression` - Evaluate x64dbg expression (param: `expression`) — supports math, symbols, registers, memory dereferences like `[rsp+8]`

### Cross-References (1 tool)
- `xref_get` - Get cross-references to an address (param: `address`)

### Function Analysis (2 tools)
- `function_list` - List all recognized functions (optional: `module` filter)
- `function_get` - Get function boundaries at address (param: `address`)

### Assembler (1 tool)
- `assembler_assemble` - Assemble instruction to bytes (params: `instruction`, `address`; optional: `write_to_memory`)

### Bookmarks (3 tools)
- `bookmark_set` - Set bookmark (param: `address`)
- `bookmark_delete` - Delete bookmark (param: `address`)
- `bookmark_list` - List all bookmarks

### Patch Management (2 tools)
- `patch_list` - List all applied byte-level patches
- `patch_restore` - Restore original bytes at address (param: `address`)

## x64dbg Log Format Syntax

When using `breakpoint_set_log`, format strings use these placeholders:
- `{REG}` - Register value in hex (e.g., `{RAX}`, `{RCX}`)
- `{REG:x}` - Explicit hex format
- `{[ADDR]}` - Dereference pointer at address
- `{[ADDR]:us}` - Read as Unicode string
- `{[ADDR]:as}` - Read as ASCII string
- `{[REG+OFFSET]}` - Register + offset dereference

### x64 fastcall parameter logging
```
"FuncName: p1={RCX} p2={RDX} p3={R8} p4={R9} ret={[RSP]:x}"
```

### x86 stdcall parameter logging
```
"FuncName: p1={[ESP+4]:x} p2={[ESP+8]:x} p3={[ESP+C]:x}"
```

## Common Reverse Engineering Patterns

### Identifying Calling Conventions
- **x64 fastcall** (Windows): RCX, RDX, R8, R9, then stack. Return in RAX.
- **x86 cdecl**: All params on stack, caller cleans. Return in EAX.
- **x86 stdcall** (WinAPI): All params on stack, callee cleans. Return in EAX.
- **x86 thiscall** (C++): ECX = this pointer, rest on stack.

### Recognizing Crypto Constants
| Constant | Algorithm |
|----------|-----------|
| 0x67452301, 0xEFCDAB89 | MD5 / SHA-1 init |
| 0x6A09E667, 0xBB67AE85 | SHA-256 init |
| 0x61707865 ("expa") | ChaCha20 / Salsa20 |
| 0xEDB88320 | CRC32 (reflected) |
| 0x04C11DB7 | CRC32 (normal) |
| 0x9E3779B9 | TEA / XTEA golden ratio |

### Windows Debug Heap Fill Patterns
| Pattern | Meaning |
|---------|---------|
| 0xCCCCCCCC | Uninitialized stack (MSVC debug) |
| 0xCDCDCDCD | Uninitialized heap (MSVC debug) |
| 0xDDDDDDDD | Freed heap memory |
| 0xFEEEFEEE | Freed heap (Windows debug heap) |
| 0xFDFDFDFD | Heap guard bytes (buffer boundaries) |
| 0xBAADF00D | LocalAlloc uninitialized |
| 0xDEADBEEF | Common debug marker |

### Anti-Debug Detection Points
- `kernel32.IsDebuggerPresent` - Check PEB.BeingDebugged
- `ntdll.NtQueryInformationProcess` - ProcessDebugPort (0x7)
- `kernel32.CheckRemoteDebuggerPresent`
- PEB.NtGlobalFlag (0x70 = debugger attached)
- Timing checks: `rdtsc`, `QueryPerformanceCounter`, `GetTickCount`
