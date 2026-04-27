# x64dbg MCP Tool Reference

Quick reference for all 78 MCP tools organized by category.

## Tool Name Convention

- MCP tool names use **underscores**: `debug_get_state`
- JSON-RPC method names use **dots**: `debug.get_state`
- In Claude Code commands, always use the underscore form.

## Debug Control

| Tool | Parameters | Description |
|------|-----------|-------------|
| `debug_get_state` | - | Get current state (paused/running/stopped) |
| `debug_run` | - | Continue execution |
| `debug_pause` | - | Break execution |
| `debug_step_into` | - | Step into calls |
| `debug_step_over` | - | Step over calls |
| `debug_step_out` | - | Step out of function |
| `debug_run_to` | `address` | Run to specific address |
| `debug_restart` | - | Restart debug session |
| `debug_stop` | - | Stop debugging |

## Registers

| Tool | Parameters | Description |
|------|-----------|-------------|
| `register_get` | `name` | Read one register |
| `register_set` | `name`, `value` | Write register |
| `register_list` | `general_only?` | List all registers |
| `register_get_batch` | `names[]` | Read multiple registers |

## Memory

| Tool | Parameters | Description |
|------|-----------|-------------|
| `memory_read` | `address`, `size`, `encoding?` | Read memory bytes |
| `memory_write` | `address`, `data`, `encoding?` | Write memory bytes |
| `memory_search` | `pattern`, `start?`, `end?`, `max_results?` | Search memory for pattern |
| `memory_get_info` | `address` | Get region info (protection, type) |
| `memory_enumerate` | - | Full memory map |
| `memory_allocate` | `size` | Allocate memory in target |
| `memory_free` | `address` | Free allocated memory |

## Breakpoints

| Tool | Parameters | Description |
|------|-----------|-------------|
| `breakpoint_set` | `address`, `type?`, `enabled?` | Set breakpoint |
| `breakpoint_delete` | `address` | Remove breakpoint |
| `breakpoint_enable` | `address` | Enable breakpoint |
| `breakpoint_disable` | `address` | Disable breakpoint |
| `breakpoint_toggle` | `address` | Toggle state |
| `breakpoint_list` | - | List all breakpoints |
| `breakpoint_get` | `address` | Get breakpoint details |
| `breakpoint_delete_all` | - | Remove all breakpoints |
| `breakpoint_set_condition` | `address`, `condition` | Set conditional expression |
| `breakpoint_set_log` | `address`, `log_text` | Set logging message |
| `breakpoint_reset_hitcount` | `address` | Reset hit counter |

## Disassembly

| Tool | Parameters | Description |
|------|-----------|-------------|
| `disassembly_at` | `address`, `count?` | Disassemble N instructions |
| `disassembly_function` | `address` | Disassemble entire function |
| `disassembly_range` | `start`, `end`, `max_instructions?` | Disassemble address range |

## Symbols

| Tool | Parameters | Description |
|------|-----------|-------------|
| `symbol_resolve` | `symbol` | Name to address |
| `symbol_from_address` | `address` | Address to name |
| `symbol_search` | `pattern` | Search by pattern |
| `symbol_list` | `module?` | List all symbols |
| `symbol_set_label` | `address`, `label` | Set user label |
| `symbol_set_comment` | `address`, `comment` | Set comment |
| `symbol_get_comment` | `address` | Get comment |

## Modules

| Tool | Parameters | Description |
|------|-----------|-------------|
| `module_list` | - | All loaded modules |
| `module_get` | `module` | Module details |
| `module_get_main` | - | Main executable module |
| `module_get_exports` | `module` | List module exports |
| `module_get_imports` | `module` | List module imports |

## Threads

| Tool | Parameters | Description |
|------|-----------|-------------|
| `thread_list` | - | All threads |
| `thread_get_current` | - | Current thread info |
| `thread_switch` | `thread_id` | Switch to thread |
| `thread_get` | `thread_id` | Thread details |
| `thread_suspend` | `thread_id` | Suspend thread |
| `thread_resume` | `thread_id` | Resume thread |
| `thread_get_count` | - | Thread count |

## Stack

| Tool | Parameters | Description |
|------|-----------|-------------|
| `stack_get_trace` | - | Call stack trace |
| `stack_read_frame` | `address`, `size` | Read stack frame data |
| `stack_get_pointers` | - | RSP/RBP values |
| `stack_is_on_stack` | `address` | Check if address is on stack |

## Dump

| Tool | Parameters | Description |
|------|-----------|-------------|
| `dump_module` | `module`, `output_path`, `oep?`, `rebuild_pe?`, `auto_detect_oep?` | Dump PE module to file |
| `dump_memory_region` | `address`, `size`, `output_path`, `as_raw_binary?` | Dump raw memory |
| `dump_analyze_module` | `module?` | PE analysis with packer detection |
| `dump_detect_oep` | `module` | Detect Original Entry Point |
| `dump_get_dumpable_regions` | `module_base?` | List dumpable regions |

## Script

| Tool | Parameters | Description |
|------|-----------|-------------|
| `script_execute` | `command` | Execute x64dbg command |
| `script_execute_batch` | `commands[]`, `stop_on_error?` | Batch commands |
| `script_get_last_result` | - | Last command result |

## Context Snapshots

| Tool | Parameters | Description |
|------|-----------|-------------|
| `context_get_snapshot` | `include_memory?`, `include_stack?` | Full state capture |
| `context_get_basic` | - | Quick register + state |
| `context_compare_snapshots` | `snapshot1`, `snapshot2` | Diff two snapshots |

## Expression Evaluation

| Tool | Parameters | Description |
|------|-----------|-------------|
| `eval_expression` | `expression` | Evaluate x64dbg expression (math, symbols, `[rsp+8]`, etc.) |

## Cross-References

| Tool | Parameters | Description |
|------|-----------|-------------|
| `xref_get` | `address` | Get cross-references (callers/jumps/data) to address |

## Function Analysis

| Tool | Parameters | Description |
|------|-----------|-------------|
| `function_list` | `module?` | List all recognized functions |
| `function_get` | `address` | Get function boundaries at address |

## Assembler

| Tool | Parameters | Description |
|------|-----------|-------------|
| `assembler_assemble` | `instruction`, `address`, `write_to_memory?` | Assemble instruction to bytes |

## Bookmarks

| Tool | Parameters | Description |
|------|-----------|-------------|
| `bookmark_set` | `address` | Set bookmark |
| `bookmark_delete` | `address` | Delete bookmark |
| `bookmark_list` | - | List all bookmarks |

## Patch Management

| Tool | Parameters | Description |
|------|-----------|-------------|
| `patch_list` | - | List all applied patches |
| `patch_restore` | `address` | Restore original bytes at address |
