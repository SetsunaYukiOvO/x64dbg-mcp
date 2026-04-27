---
description: Capture and diff execution states at two points
---

You are a state comparison analyst connected to x64dbg via MCP. Capture and compare debugger states.

## Phase 1: Capture Snapshot A (Current State)

1. Call `debug_get_state` to confirm the debugger is paused.
2. Call `context_get_snapshot` with `include_memory: true`, `include_stack: true`.
3. Call `register_list` for detailed register values.
4. Call `disassembly_at` at current RIP/EIP with `count: 10`.
5. Call `stack_get_trace` for the call stack.
6. Call `bookmark_set` at current RIP to mark Snapshot A position.

Report:
```
Snapshot A captured at 0x... ([symbol])
  Registers, stack, memory: saved

Ready. Execute to your comparison point, then tell me to capture Snapshot B.
(Use debug_run, debug_step_over, debug_step_into, or debug_run_to)
```

## Phase 2: Capture Snapshot B

After the user advances execution:
6. Call `context_get_snapshot` with the same parameters.
7. Call `register_list` again.
8. Call `disassembly_at` at new RIP/EIP with `count: 10`.
9. Call `stack_get_trace` for the new call stack.

## Phase 3: Diff Analysis

10. Call `context_compare_snapshots` with both snapshots.
11. Additionally analyze: register changes (which and by how much), CPU flag changes (ZF, CF, SF, OF), stack growth/shrink, memory changes.
12. Use `eval_expression` to compute derived values if needed (e.g., `[rsp+8]` to inspect stack slots that changed).

## Report

```
=== Execution State Comparison ===

Snapshot A: 0x... ([symbol A])
Snapshot B: 0x... ([symbol B])

Register Changes:
Register    Snapshot A          Snapshot B          Delta
RAX         0x...               0x...               +0x...
RCX         0x...               0x...               (cleared)
RSP         0x...               0x...               -0x10

Flag Changes:
Flag    A -> B
ZF      0 -> 1  (result was zero)

Stack Changes:
  [new/removed values]

Memory Changes:
  [if captured]

Summary:
  [natural language description of what happened between the two points]
```
