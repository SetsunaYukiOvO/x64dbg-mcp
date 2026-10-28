# Review fixes, 2026-10-28

Baseline: `70e65271336a79975abc5c2e5cd3228d4d54d414`. Each R01–R19 fix has its own commit on `dev`.

| Finding | Change | Regression coverage |
| --- | --- | --- |
| R01 | Shared PE validation checks signed offsets, native optional headers, declared header sizes and complete section tables before access. | `pe_validation`, `pe_rebuild`; Windows MSVC AddressSanitizer |
| R02 | Parse complete HTTP field names and the final header; reject duplicate fields, malformed lengths and unsupported transfer encodings. | `http_headers`, real socket authentication/Origin checks in `http_transport` |
| R03 | Check memory-write permission before accessing the patch restoration SDK. | `runtime_configuration` verifies denial causes zero SDK calls |
| R04 | Validate condition type and script permission before creating any breakpoint; propagate condition setup failure. | `breakpoint_address_entry_x86` |
| R05 | Execute steps directly, wait for a new pause callback sequence, and serialize debugger operations. | `debug_execution`: delayed start, concurrent steps, configurable timeout |
| R06 | Use `run <address>` and wait for a new stop before checking the target IP. | `debug_execution` |
| R07 | Validate PID, TID and timeout values before narrowing; consume complete decimal/hex strings. | `uint32_validation` |
| R08 | Announce a random legacy SSE session endpoint; unicast responses to its connection. Plain HTTP responses remain inline. Protect server/socket lifetime during broadcasts. | Two real SSE clients, inline HTTP, invalid session and shutdown broadcast in `http_transport` |
| R09 | Bound live tasks/connections, cap header size, enforce absolute header/body deadlines and handle task/thread creation failure. | Stalled connections, saturation, timeout recovery and restart in `http_transport` |
| R10 | Use uncached breakpoint list snapshots, including disabled entries, for queries and metadata operations. Software/hardware/memory priority is deterministic. | `breakpoint_snapshot`, including mixed types and delete-all |
| R11 | Enable/disable existing breakpoints using commands for their actual type; expose optional type selection. | `breakpoint_snapshot`, mixed-type selection |
| R12 | A memory-image dump uses its runtime base as ImageBase, consistent with relocation-adjusted bytes. | `pe_rebuild` |
| R13 | Carry the previous chunk's pattern-length-minus-one tail; clear overlap after failed reads. | `memory_search`: boundaries, wildcards, range/result limits, read gaps |
| R14 | Require valid hexadecimal characters and complete byte pairs. | `hex_bytes`, `memory_search` |
| R15 | Compare complete segment register names when filtering general registers. | `general_registers_x64`, native `general_registers_x86` |
| R16 | Dispatch real register, module, thread, stack and breakpoint collection; propagate failures. | `context_snapshot` |
| R17 | Implement all eight advertised resource templates, URI decoding, parameter bounds and method dispatch. | `resource_templates` |
| R18 | Use SDK `SYMBOLINFOCPP` ownership handling on success and failure. | `symbol_ownership`: allocated and borrowed names |
| R19 | Apply runtime configuration and validate numeric limits. | Configuration tests below |

## Configuration read points and execution points

| Configuration | Runtime consumer | Validation |
| --- | --- | --- |
| `server.max_connections` | HTTP accept loop, 128 default, 1–1024 | `http_transport` |
| `timeout.request_timeout_ms` | Absolute HTTP receive budget; MCP batch/request budget; pause/run-to/attach defaults | `http_transport`, `runtime_configuration` |
| `timeout.step_timeout_ms` | New-pause wait after a step | `debug_execution`, `runtime_configuration` |
| `timeout.memory_read_timeout_ms` | Page-by-page memory read budget | `memory_search` |
| `features.enable_notifications` | Callback initialization and notification broadcast | `runtime_configuration`, `http_transport` |
| `features.enable_heartbeat`, `heartbeat_interval_seconds` | Both SSE transports, monotonic timing | `http_transport` |
| `features.enable_batch_requests` | HTTP MCP array handling and JSON-RPC batch dispatch | `http_transport` |
| `logging.enabled`, `level`, `file`, `console_output` | Plugin initialization/config reload, logger setup | Runtime setup plus `runtime_configuration` |
| `logging.max_file_size_mb` | Bounded log rotation with one `.1` backup | `runtime_configuration` |

Timeouts are cooperative around synchronous SDK calls. An in-progress SDK call cannot safely be force-cancelled; a request timeout does not undo side effects. Receive deadlines are absolute and cannot be extended by sending occasional bytes. Invalid/nonpositive timeout values fall back to documented defaults; the configuration editor rejects invalid ranges.

The dump format remains a reconstructed memory image. It does not reconstruct every loader transformation, import binding or original file layout, and does not guarantee that every protected executable will run as a standalone dump.

## Validation results

- Windows x64 Release plugin build: passed; CTest: **27/27 passed**.
- Windows x86 Release plugin build: passed; CTest: **26/26 passed**.
- MSVC AddressSanitizer x64: **PE validation and rebuild executables passed**.
- The extra x64 test runs the x86 register-selection path against an x64 SDK structure. The native x86 build tests its real SDK layout and does not try to compile x64-only SDK fields.

## Validation boundary

Validation uses Windows, Visual Studio 2022, the bundled SDK headers and native x64/x86 plugin builds. Regression tests exercise production functions with explicit SDK replacements, and HTTP/SSE tests use real Winsock connections. PE validation/rebuild tests also run with MSVC AddressSanitizer.

Real x64dbg/x32dbg end-to-end execution and unloading a live debugger plugin are not covered by these tests. The HTTP server shutdown/broadcast race is tested independently of the debugger host.

Upstream command references: [run](https://help.x64dbg.com/en/latest/commands/debug-control/run.html), [hardware breakpoint enable](https://help.x64dbg.com/en/latest/commands/breakpoint-control/EnableHardwareBreakpoint.html), [breakpoint commands](https://help.x64dbg.com/en/latest/commands/breakpoint-control/index.html).
