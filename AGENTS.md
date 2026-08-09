# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

# My Custom Instructions
- Perform all code changes on the `dev` branch. Keep `main` for reviewed integration and release changes; do not develop directly on `main`.
- 使用中文回复
- 代码注释使用英文

## Build and development commands

### Prerequisites
- Windows 10/11, Visual Studio 2022 (Desktop C++ workload), CMake >= 3.15
- vcpkg with `nlohmann-json` available (repo uses `vcpkg.json`)
- `VCPKG_ROOT` environment variable (defaults to `C:\vcpkg` in `build.bat`)

### Build
- Both architectures (Release): `./build.bat`
- Clean rebuild: `./build.bat --clean`
- x64 only: `./build.bat --x64-only`
- x86 only: `./build.bat --x86-only`
- Debug: `./build.bat --debug`

Outputs go to `dist/`: `x64dbg_mcp.dp64` and `x32dbg_mcp.dp32`.

### Manual CMake (x64 example)
```
cmake -B build_x64 -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DXDBG_ARCH=x64
cmake --build build_x64 --config Release -j
```
For x86, use `-A Win32`, `-DVCPKG_TARGET_TRIPLET=x86-windows`, `-DXDBG_ARCH=x86`.

### Tests
No test suite exists yet. CMake conditionally enables tests (`BUILD_TESTS=ON`) if a `tests/` directory is present.

## Architecture overview

x64dbg/x32dbg plugin (C++17, shared library) that exposes debugger capabilities through MCP (Model Context Protocol) over HTTP + SSE, with JSON-RPC 2.0 as the internal execution protocol.

### Request flow
```
MCP client
  → HTTP POST /rpc (or /message, /messages, /)   |  GET /sse for events
  → MCPHttpServer (Winsock, src/communication/)
      handles MCP methods: initialize, tools/list, tools/call, resources/*, prompts/*
  → tools/call: MCPToolRegistry resolves tool → JSON-RPC method name
  → MethodDispatcher routes to handler function
      (PermissionChecker gates write/execute operations)
  → Handler (src/handlers/) → Business layer (src/business/) → x64dbg SDK
```

### Plugin lifecycle
- DLL entrypoint: `src/PluginEntry.cpp` exports `pluginit`, `plugsetup`, `plugstop` (via `src/plugin.def`).
- `pluginit`: loads config, initializes logger, registers all methods/callbacks.
- `plugsetup`: builds plugin menu (Start/Stop server, config editor, about).
- MCP HTTP server starts via menu or auto-start config flag (`features.auto_start_mcp_on_plugin_load`).

### Method registration (adding a new capability)
All registration happens in `RegisterAllMethods()` in `PluginEntry.cpp`:
1. **MCP registries** — `MCPToolRegistry`, `MCPResourceRegistry`, `MCPPromptRegistry` each have `RegisterDefault*()` that define tool/resource/prompt metadata.
2. **Handler-based methods** — each `*Handler` class has a static `RegisterMethods()` that registers dotted JSON-RPC names (e.g. `debug.run`) with `MethodDispatcher`.
3. **Inline registrations** — `script.*` and `context.*` methods are registered as lambdas directly in `RegisterAllMethods()` (they use standalone `ScriptHandler`/`ContextHandler` classes that don't follow the `RegisterMethods()` pattern).

To add a new tool:
1. Add JSON-RPC handler method (new or existing Handler class).
2. Register the JSON-RPC method in `RegisterAllMethods()`.
3. Add MCP tool metadata in `MCPToolRegistry::RegisterDefaultTools()` — tool names use underscore style (`debug_get_state`), JSON-RPC uses dots (`debug.get_state`).
4. If the tool mutates state, add permission checks via `PermissionChecker`.

### Name mapping
- MCP tool name: `debug_get_state` (underscores)
- JSON-RPC method: `debug.get_state` (dots)
- Mapping defined in `MCPToolRegistry::RegisterDefaultTools()` (`core/MCPToolRegistry.cpp`)

### SDK abstraction
- `src/core/X64DBGBridge.h` is the single include point for x64dbg SDK headers. It provides real SDK access when `XDBG_SDK_AVAILABLE` is defined (always true in CMake builds), and stub/mock definitions otherwise. Bundled SDK lives in `include/x64dbg-pluginsdk/`.
- `duint` type is `uint64_t` on x64, `uint32_t` on x86 — controlled by `XDBG_ARCH_X64`/`XDBG_ARCH_X86` compile definitions.

### Eventing
- x64dbg callbacks (breakpoint, exception, module load/unload, process create/exit) registered in `PluginEntry.cpp`.
- Forward to `EventCallbackHandler`, which emits MCP notifications via `MCPHttpServer::BroadcastNotification()` to all SSE clients.

### Configuration
- `config.json` in the plugin directory (auto-created with defaults if missing).
- Sections: `server` (address/port), `permissions` (write/execute flags + allowed method wildcards), `logging`, `timeout`, `features` (auto-start, heartbeat, batch requests).
- Managed by singleton `core/ConfigManager`.
- UI editor available via plugin menu (`ui/ConfigEditor`).

## Notes for safe modifications
- Keep x64/x86 dual-target behavior intact: `XDBG_ARCH` CMake variable drives compile definitions, SDK lib selection, output name (`x64dbg_mcp.dp64` vs `x32dbg_mcp.dp32`), and plugin directory name.
- All singletons (`ConfigManager`, `MethodDispatcher`, `PermissionChecker`, registries, managers) — no multi-instance support.
- Preserve permission checks for any write/execute operations (memory, registers, scripts, breakpoints).
