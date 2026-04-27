---
description: Set up logging breakpoints on Windows API categories
argument-hint: [category]
---

You are an API monitoring specialist connected to x64dbg via MCP. Set up monitoring for API calls.

"$1" may specify a category: file, network, registry, crypto, process, memory. If empty, set up broad monitoring.

## API Categories

**File**: `kernel32.CreateFileA/W`, `ReadFile`, `WriteFile`, `DeleteFileA/W`, `FindFirstFileA/W`
**Network**: `ws2_32.connect`, `send`/`recv`, `wininet.InternetOpenA/W`, `InternetConnectA/W`, `HttpSendRequestA/W`
**Registry**: `advapi32.RegOpenKeyExA/W`, `RegQueryValueExA/W`, `RegSetValueExA/W`, `RegDeleteKeyA/W`
**Crypto**: `advapi32.CryptEncrypt`/`CryptDecrypt`, `bcrypt.BCryptEncrypt`/`BCryptDecrypt`, `BCryptHash`
**Process**: `kernel32.CreateProcessA/W`, `CreateRemoteThread`, `VirtualAllocEx`, `WriteProcessMemory`
**Memory**: `kernel32.VirtualAlloc`, `VirtualProtect`, `LoadLibraryA/W`, `GetProcAddress`

## Pre-Check

First, call `module_get_imports` on the main module. This instantly shows which APIs the binary actually imports — you only need to set breakpoints on APIs that appear in the import table (or are dynamically resolved via GetProcAddress).

## Setup Steps

For each target API in the selected category:

1. Call `symbol_resolve` with the full name (e.g., `kernel32.CreateFileW`) to get the address.
2. If resolved, call `breakpoint_set` at the address.
3. Call `breakpoint_set_log` with a descriptive format string:
   - x64: `"CreateFileW: path={[RCX]:us} access={RDX:x} share={R8:x}"`
   - x86: `"CreateFileW: path={[[ESP+4]]:us} access={[ESP+8]:x}"`

## Report

```
=== API Monitoring Setup ===

Category: [selected or "All"]
Target: [binary name]

Monitored APIs ([count]):

  [Category Name]:
    [x] APIName    @ 0x... (logging: [parameter description])
    [ ] APIName    - not found in imports
    ...

Breakpoints Set: [count]

Instructions:
  1. Run the target with debug_run
  2. API calls will be logged in x64dbg's log window
  3. To pause on a specific call, modify the breakpoint to remove log-only mode

Tips:
  - Add conditions with breakpoint_set_condition to filter noise
  - Watch for: CreateRemoteThread + WriteProcessMemory = code injection
  - Network: connect -> send reveals C2 communication
```
