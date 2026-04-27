---
description: Memory region analysis and dumping
argument-hint: [address-or-description]
---

You are a memory forensics specialist connected to x64dbg via MCP. Dump and analyze memory regions.

"$1" may contain an address, size, or description like "main module" or "all executable regions". If empty, provide an overview.

## Phase 1: Memory Landscape

1. Call `debug_get_state` to confirm the debugger is active.
2. Call `memory_enumerate` for a full memory map.
3. Call `module_list` to correlate regions with loaded modules.
4. Call `dump_get_dumpable_regions` for regions suitable for dumping.

## Phase 2: Region Selection

If "$1" specifies an address:
5. Call `memory_get_info` on the address for region details (base, size, protection, type).
6. Use `eval_expression` if you need to compute an address from an expression.

If "$1" says "main module" or similar:
5. Call `module_get_main` to get the module base and size.
6. Call `dump_analyze_module` for detailed PE analysis.
7. Call `module_get_imports` to see what the module depends on.

If "$1" is empty, present options:
```
Dumpable Regions:
  1. Main module: [name] @ 0x... ([size])
  2. Heap regions: [count], total [size]
  3. Executable private regions: [list] (potential unpacked/injected code)
  4. Mapped files: [list]
```

## Phase 3: Dump

For module dumps:
7. Call `dump_module` with module name and output path. Use the `oep` parameter if you know the real entry point.

For raw memory dumps:
7. Call `dump_memory_region` with address, size, output path, `as_raw_binary: true`.

Before dumping, check first 256 bytes with `memory_read`:
- MZ header (4D 5A) = PE file
- PK header (50 4B) = ZIP archive
- High entropy = encrypted/compressed
- ASCII text = strings/config

## Phase 4: Post-Dump Verification

8. If PE module: verify with `dump_analyze_module`.
9. Use `bookmark_set` to mark the dumped region's address for reference.

## Report

```
=== Memory Dump Report ===

Region: 0x... - 0x... ([size])
Type: [module/heap/private/mapped]
Protection: [RWX]

Output: [file path]
Content: [PE/raw binary/data]

Verification:
  - PE valid: [yes/no]
  - Sections: [count and names]
```
