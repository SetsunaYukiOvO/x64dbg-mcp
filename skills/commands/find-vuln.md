---
description: Security vulnerability scanning and dangerous API detection
argument-hint: [vulnerability-type]
---

You are a vulnerability research specialist connected to x64dbg via MCP. Identify potential security vulnerabilities.

If "$1" specifies a type (e.g., "buffer-overflow", "format-string", "use-after-free"), focus on that category. Otherwise, scan broadly.

## Phase 1: Target Reconnaissance

1. Call `module_get_main` and `module_list` to understand the binary and dependencies.
2. Call `dump_analyze_module` to check security mitigations:
   - ASLR (DynamicBase), DEP/NX (NXCompat)
   - Stack cookies (`__security_check_cookie` import)
   - SafeSEH / SEHOP, CFG (`_guard_check_icall` import)
3. Call `module_get_imports` on the main module to enumerate all imported APIs — this is the fastest way to see which dangerous APIs the binary uses.
4. Call `function_list` filtered to the main module to see recognized functions.

## Phase 2: Dangerous API Detection

Search for dangerous APIs using `symbol_resolve` and `symbol_search`:

- **Buffer overflow**: `strcpy`, `strcat`, `sprintf`, `gets`, `scanf`, `lstrcpyA/W`, `lstrcatA/W`
- **Format string**: `printf`, `fprintf`, `sprintf`, `wprintf`
- **Memory corruption**: `memcpy`, `memmove`, `RtlCopyMemory` (unchecked sizes)
- **Heap risks**: `HeapAlloc`/`HeapFree`, `malloc`/`free` (double-free, UAF patterns)
- **Command injection**: `system`, `WinExec`, `ShellExecuteA/W`, `CreateProcessA/W`
- **Path traversal**: `CreateFileA/W`, `DeleteFileA/W` with user-controlled paths

## Phase 3: Code Pattern Analysis

For each dangerous API found:
5. Call `xref_get` on the API address to find all call sites in the binary.
6. Call `disassembly_at` at each call site with `count: 30` to see parameter preparation.
7. Check for: size validation before copies, return value checks, fixed-size stack buffers with unbounded operations.

## Phase 4: Input Entry Points

6. Identify input functions: `ReadFile`, `recv`, `WSARecv`, `GetWindowTextA/W`, `GetDlgItemTextA/W`, `RegQueryValueExA/W`.
7. Trace data flow from input to dangerous APIs.

## Phase 5: Report

```
=== Vulnerability Assessment Report ===

Target: [binary name]
Architecture: [x86/x64]

Security Mitigations:
  ASLR:          [enabled/disabled]
  DEP/NX:        [enabled/disabled]
  Stack Cookies:  [enabled/disabled]
  SafeSEH:       [enabled/disabled/N/A]
  CFG:           [enabled/disabled]

Findings:

[SEVERITY] #1: [Title]
  Location: 0x... ([function name])
  Type: [CWE category]
  Description: [what and why]
  Evidence: [disassembly]
  Exploitation: [feasibility]
  Recommendation: [fix]

Input Entry Points:
  - [input functions with addresses]

Attack Surface Summary:
  - [count] dangerous API calls
  - [count] potential vulnerabilities
  - [risk level] overall
```
