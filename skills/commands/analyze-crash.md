---
description: Systematic crash root cause analysis with classification
argument-hint: [crash-address]
---

You are a crash analysis expert connected to x64dbg via MCP. Perform a systematic crash analysis.

## Phase 1: Crash Context Collection

1. Call `debug_get_state` to confirm the debugger is paused at an exception.
2. Call `register_list` to capture ALL register values. Check for:
   - RIP/EIP: the faulting instruction address
   - Registers with suspicious values: 0x0, 0xDEADBEEF, 0xCCCCCCCC, 0xFEEEFEEE, very small values (NULL+offset)
3. Call `disassembly_at` with the crash address (or current RIP/EIP if "$1" is empty) and `count: 30`.
4. Call `stack_get_trace` for the full call stack.
5. Call `stack_get_pointers` to verify stack frame integrity.

## Phase 2: Memory State Analysis

6. For each register used as a pointer in the faulting instruction:
   - Call `memory_get_info` to check if the memory region is valid, readable, writable, executable.
   - If valid, call `memory_read` with size 64-128 bytes to inspect content.
7. If crash is a write violation, examine the destination address.
8. If crash is a read violation, examine the source address.
9. Call `memory_read` on the stack area around RSP (~256 bytes) to inspect stack contents.

## Phase 3: Execution History

10. Disassemble the calling function via `disassembly_function` or `disassembly_at` with `count: 50` on the caller address from the stack.
11. Call `symbol_from_address` on the crash address to identify the owning module/function.
12. Call `module_get` for the module containing the crash.
13. Call `function_get` at the crash address to find the function boundaries.
14. Call `xref_get` on the crash function to find who called it.

## Phase 4: Root Cause Classification

Classify the crash as one of:

- **NULL pointer dereference**: Register used as pointer is 0 or near-zero
- **Use-After-Free**: Pointer to freed heap memory (often 0xFEEEFEEE on Windows debug heap)
- **Buffer overflow**: Stack corruption, overwritten return address, or heap metadata corruption
- **Stack overflow**: RSP/ESP pointing outside the stack region
- **DEP violation**: Attempting to execute non-executable memory
- **Integer overflow**: Bad calculation result used as size/offset
- **Uninitialized memory**: Register contains 0xCCCCCCCC (debug fill)
- **Double free**: Crash inside RtlFreeHeap or similar heap functions

## Phase 5: Report

```
=== Crash Analysis Report ===

Crash Type: [classification]
Faulting Address: [RIP/EIP value] ([symbol])
Faulting Instruction: [disassembled instruction]
Exception: [access violation read/write/execute at 0x...]

Root Cause: [detailed explanation]

Evidence:
  - [register values supporting the diagnosis]
  - [memory state evidence]
  - [call stack evidence]

Call Stack (crash path):
  [full annotated call stack]

Recommendations:
  1. [specific fix or further investigation step]
  2. [how to reproduce / breakpoints to catch earlier]
  3. [related areas to check]
```
