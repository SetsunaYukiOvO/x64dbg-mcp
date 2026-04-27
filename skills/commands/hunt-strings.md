---
description: String searching with cross-reference analysis
argument-hint: [search-pattern]
---

You are a string analysis specialist connected to x64dbg via MCP. Search for interesting strings in the binary.

## Phase 1: Context

1. Call `module_get_main` to identify the target.
2. Call `module_list` for all loaded modules and address ranges.
3. Call `dump_analyze_module` to understand section layout (.rdata, .data, .rsrc).

## Phase 2: Search

If "$1" is provided, search for that specific pattern:
4. Call `memory_search` with "$1" as pattern in the main module range.
   - Also search for UTF-16LE encoding (wide strings).
5. For each match, call `memory_read` at the address with `size: 256` for full context.

If "$1" is empty, search for high-value categories:
4. Search using `memory_search` for:
   - **Credentials**: password, secret, token, api_key, auth
   - **Network**: http://, https://, ftp://, .com, .net
   - **File paths**: C:\, .exe, .dll, .dat, .cfg, .ini
   - **Debug/Error**: error, fail, debug, assert, exception
   - **Registry**: HKEY_, SOFTWARE\, CurrentVersion
   - **Crypto**: AES, RSA, SHA, MD5, encrypt, decrypt

## Phase 3: Cross-Reference Analysis

For each interesting string found:
6. Note the string address.
7. Call `xref_get` on the string address to find direct references from code.
8. If `xref_get` returns no results, fall back to `memory_search` for the address bytes (little-endian) in code sections.
9. For each cross-reference, call `disassembly_at` with `count: 15` to see how the string is used.
10. Call `symbol_from_address` to identify the referencing function.
11. Call `function_get` at the referencing address to see the full function context.

## Phase 4: Report

```
=== String Analysis Report ===

Target: [binary name]
Search: [pattern or "comprehensive scan"]

=== High Priority ===

[CREDENTIAL] @ 0x...
  String: "..."
  Referenced by: 0x... ([function])
  Context: [how it's used]

[NETWORK] @ 0x...
  String: "https://..."
  Referenced by: 0x... ([function])

=== By Category ===

Credentials: [count]
URLs: [count]
File Paths: [count]
Debug Messages: [count]
Registry Keys: [count]

=== Recommendations ===
  1. [strings warranting deeper investigation]
  2. [breakpoints to set]
  3. [security concerns]
```
