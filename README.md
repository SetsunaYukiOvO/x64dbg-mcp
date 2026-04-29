# x64dbg MCP Server Plugin

English | [中文](docs/README_CN.md)

A Model Context Protocol (MCP) server implementation for x64dbg and x32dbg, enabling remote debugging through a JSON-RPC 2.0 interface. This plugin allows external applications and AI agents to interact with the debugger programmatically.

**Now supports both x64 and x86 architectures!**

## Features

- **Full MCP Specification Compliance**: Implements all three core MCP building blocks
  - **Tools (79)**: AI-invokable debugging functions
  - **Resources (7 + 8 templates)**: Application-controlled context data sources
  - **Prompts (10)**: User-guided debugging workflow templates
  
- **JSON-RPC 2.0 Protocol**: Standard, language-agnostic interface
- **HTTP + SSE Communication**: Modern web-based integration via Server-Sent Events

- **Tools - AI-Controlled Debugging (79 functions)**: 
  - Execution control (init/run/pause/step/run_to/restart/stop)
  - Memory read/write/search/allocate
  - Register access (50+ registers including GPR, SSE, AVX)
  - Breakpoint management (software, hardware, memory, conditional, logging)
  - Disassembly and symbol resolution
  - Thread management (list, switch, suspend, resume)
  - Stack trace and analysis
  - **Dump & Unpacking** (module dump, memory dump, auto-unpacking, OEP detection, IAT reconstruction)
  - **Script execution** (execute x64dbg commands, batch operations)
  - **Context snapshots** (capture and compare debugging state)
  
- **Resources - Context Providers (7 direct + 8 templates)**:
  - Direct resources: debugger state, registers, modules, threads, memory map, breakpoints, stack
  - Resource templates: memory content, disassembly, module info, symbol resolution, function analysis
  - Read-only, application-controlled access
  
- **Prompts - Workflow Templates (10 prompts)**:
  - Crash analysis, vulnerability hunting, function tracing
  - Binary unpacking, algorithm reversing, execution comparison
  - String hunting, code patching, API monitoring
  - Debug session initialization

- **Security**: Permission-based access control
- **Extensible**: Plugin architecture for custom methods, resources, and prompts

## What's New in v1.0.6

- **New Tool**: `debug_init` — starts a new debug session by loading an executable (equivalent to x64dbg's "Run" button). Works even when no session is active, so the bot can relaunch the target after a crash/exit without a reconnect. Accepts optional `path`, `arguments`, and `current_dir`; when `path` is omitted the most recently observed debuggee path is reused.
- `debug_restart` no longer requires an active debug session — it now falls back to the cached debuggee path, so it can revive a session after the target exits or crashes.

## Previous Versions

### v1.0.5

- **Bug Fix**: `debug_restart` now works correctly — x64dbg has no `restart` script command; the tool now uses `init "<path>"` to mirror the GUI's restart behavior (PR #5 by @AMRICHASFUCK)
- **Doc Fix**: Resource count corrected to "7 direct + 8 templates" (was incorrectly listed as 15)

### v1.0.4

- 12 new tools (66 → 78): `eval_expression`, `xref_get`, `function_list`/`function_get`, `module_get_exports`/`module_get_imports`, `assembler_assemble`, `bookmark_set`/`delete`/`list`, `patch_list`/`patch_restore`
- Address parsing: all address params accept symbols, registers, and x64dbg expressions via DbgEval
- `memory_search` continuous hex format support
- Claude Code plugin (`skills/`) with 11 RE slash commands
- All 10 MCP prompts rewritten with structured multi-phase workflows
- Dump: fixed ImageBase, removed unreliable auto-unpack/IAT rebuild stubs

### v1.0.3

- Generalized unpacking logic, dump/unpack stability fixes, running-state recovery

### v1.0.2

- Automated testing critical bug fixes, build system improvements

### v1.0.1

- Thread and stack management APIs
- Enhanced error handling and logging

For complete version history, see [CHANGELOG.md](CHANGELOG.md)

## Building from Source

### Prerequisites

- **Windows 10/11** (x64)
- **CMake** 3.15 or higher
- **Visual Studio 2022** with C++ Desktop Development workload
- **vcpkg** - Package manager for C++ libraries
- **Git** - For cloning the repository

### Quick Build

The easiest way to build is using the provided build script:

```powershell
# Clone the repository
git clone https://github.com/SetsunaYukiOvO/x64dbg-mcp.git
cd x64dbg-mcp

# Build both x64 and x86 architectures (recommended)
.\build.bat

# Build only x64 architecture
.\build.bat --x64-only

# Build only x86 architecture
.\build.bat --x86-only

# Clean rebuild
.\build.bat --clean

# The script will:
# 1. Automatically detect vcpkg installation
# 2. Download dependencies (nlohmann_json)
# 3. Configure CMake for both architectures
# 4. Build using Visual Studio with parallel compilation
# 5. Copy output files to dist/ directory
```

Build script options:
```powershell
.\build.bat               # Build both x64 and x86 (Release)
.\build.bat --clean       # Clean rebuild both architectures
.\build.bat --x64-only    # Build x64 only
.\build.bat --x86-only    # Build x86 only
.\build.bat --debug       # Debug build (future support)
```

**Output files** (in `dist/` directory):
- x64 plugin: `dist\x64dbg_mcp.dp64` (~837 KB)
- x86 plugin: `dist\x32dbg_mcp.dp32` (~800 KB)

### Manual Build Steps

If you prefer manual control:

1. **Install vcpkg** (if not already installed):
```powershell
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT "C:\vcpkg"
```

2. **Clone the repository**:
```powershell
git clone https://github.com/SetsunaYukiOvO/x64dbg-mcp.git
cd x64dbg-mcp
```

3. **Configure with CMake**:
```powershell
# For x64 build
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DXDBG_ARCH=x64

# For x86 build
cmake -B build -G "Visual Studio 17 2022" -A Win32 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DXDBG_ARCH=x86
```

4. **Build**:
```powershell
cmake --build build --config Release
```

5. **Output**:
- Plugin file: `build\bin\Release\x64dbg_mcp.dp64` (approximately 611 KB)

## Installation

1. Copy the compiled plugins to their respective debugger directories:

```powershell
# For x64dbg (64-bit)
# Replace <x64dbg-path> with your actual x64dbg installation directory
copy dist\x64dbg_mcp.dp64 <x64dbg-path>\x64\plugins\

# For x32dbg (32-bit)
copy dist\x32dbg_mcp.dp32 <x64dbg-path>\x32\plugins\

# Example (if installed at C:\x64dbg):
# copy dist\x64dbg_mcp.dp64 C:\x64dbg\x64\plugins\
# copy dist\x32dbg_mcp.dp32 C:\x64dbg\x32\plugins\
```

2. (Optional) Copy the configuration file:
```powershell
# For x64dbg
mkdir <x64dbg-path>\x64\plugins\x64dbg-mcp
copy config.json <x64dbg-path>\x64\plugins\x64dbg-mcp\

# For x32dbg
mkdir <x64dbg-path>\x32\plugins\x32dbg-mcp
copy config.json <x64dbg-path>\x32\plugins\x32dbg-mcp\
```

3. Restart x64dbg/x32dbg to load the plugin

## Usage

### Starting the Server

1. Open x64dbg
2. Navigate to **Plugins → MCP Server → Start MCP HTTP Server**
3. The server will start on the configured port (default: 3000)
4. Access the server at `http://127.0.0.1:3000`

### Configuration

Edit `config.json` to customize settings:

```json
{
  "version": "1.0.5",
  "server": {
    "address": "127.0.0.1",
    "port": 3000
  },
  "permissions": {
    "allow_memory_write": true,
    "allow_register_write": true,
    "allow_script_execution": true,
    "allow_breakpoint_modification": true
  },
  "logging": {
    "enabled": true,
    "level": "info",
    "file": "x64dbg_mcp.log"
  }
}
```

### Client Example

Python client example using HTTP:

```python
import requests
import json

class MCPClient:
    def __init__(self, host='127.0.0.1', port=3000):
        self.base_url = f"http://{host}:{port}"
        self.request_id = 1
    
    def call(self, method, params=None):
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method,
            "params": params or {}
        }
        self.request_id += 1
        
        response = requests.post(
            f"{self.base_url}/rpc",
            json=request,
            headers={"Content-Type": "application/json"}
        )
        return response.json()
    
    def subscribe_events(self):
        """Subscribe to SSE events"""
        response = requests.get(
            f"{self.base_url}/sse",
            stream=True,
            headers={"Accept": "text/event-stream"}
        )
        for line in response.iter_lines():
            if line:
                yield line.decode('utf-8')

# Usage
client = MCPClient()
print(client.call("initialize"))
print(client.call("tools/list"))
print(client.call("resources/list"))
print(client.call("prompts/list"))

# Subscribe to debug events
for event in client.subscribe_events():
    print(f"Event: {event}")
```

Cursor and other MCP clients usually decide which sections to show from the `initialize` response capabilities. This server advertises `tools`, `resources`, and `prompts`, so after reconnecting you should see all three categories in the client UI.

### VS Code Integration

Configure in VS Code settings or MCP client config:

```json
{
  "mcpServers": {
    "x64dbg": {
      "url": "http://127.0.0.1:3000",
      "transport": "sse"
    }
  }
}
```

## Available Methods

### System Methods
- `system.info` - Get server information
- `system.ping` - Test connection
- `system.methods` - List all available methods

### Debug Control
- `debug.run` - Continue execution
- `debug.pause` - Pause execution
- `debug.step_into` - Step into instruction
- `debug.step_over` - Step over instruction
- `debug.step_out` - Step out of function
- `debug.get_state` - Get current debug state
- `debug.run_to` - Run to specific address
- `debug.restart` - Restart debugging session
- `debug.stop` - Stop debugging

### Register Operations
- `register.get` - Read single register
- `register.set` - Write register value
- `register.list` - List all registers
- `register.get_batch` - Read multiple registers

### Memory Operations
- `memory.read` - Read memory region
- `memory.write` - Write memory region
- `memory.search` - Search memory pattern
- `memory.get_info` - Get memory region info
- `memory.enumerate` - List all memory regions
- `memory.allocate` - Allocate memory
- `memory.free` - Free allocated memory

### Breakpoint Management
- `breakpoint.set` - Set breakpoint
- `breakpoint.delete` - Remove breakpoint
- `breakpoint.enable` - Enable breakpoint
- `breakpoint.disable` - Disable breakpoint
- `breakpoint.toggle` - Toggle breakpoint state
- `breakpoint.list` - List all breakpoints
- `breakpoint.get` - Get breakpoint details
- `breakpoint.delete_all` - Remove all breakpoints
- `breakpoint.set_condition` - Set breakpoint condition
- `breakpoint.set_log` - Set breakpoint log message
- `breakpoint.reset_hitcount` - Reset breakpoint hit count

### Disassembly
- `disassembly.at` - Disassemble at address
- `disassembly.range` - Disassemble address range
- `disassembly.function` - Disassemble entire function

### Symbol Resolution
- `symbol.resolve` - Resolve symbol to address
- `symbol.from_address` - Get symbol from address
- `symbol.search` - Search symbols by pattern
- `symbol.list` - List all symbols
- `symbol.modules` - List loaded modules
- `symbol.set_label` - Set symbol label
- `symbol.set_comment` - Set symbol comment
- `symbol.get_comment` - Get symbol comment

### Module Operations
- `module.list` - List all loaded modules
- `module.get` - Get module information
- `module.get_main` - Get main module

### Thread Operations
- `thread.list` - List all threads
- `thread.get_current` - Get current thread
- `thread.get` - Get thread information
- `thread.switch` - Switch to thread
- `thread.suspend` - Suspend thread
- `thread.resume` - Resume thread
- `thread.get_count` - Get thread count

### Stack Operations
- `stack.get_trace` - Get stack trace
- `stack.read_frame` - Read stack frame
- `stack.get_pointers` - Get stack pointers (RSP, RBP)
- `stack.is_on_stack` - Check if address is on stack

For complete method signatures and examples, see the inline documentation in the source code or use the `system.methods` API call.

## Architecture

The plugin is organized into four layers:

1. **Communication Layer**: HTTP server with SSE support for real-time events
2. **Protocol Layer**: JSON-RPC and MCP protocol parsing, validation, dispatching
3. **Business Layer**: Debugging operations, memory management, symbol resolution
4. **Plugin Layer**: x64dbg integration, event handling, callback management

### Key Components

- **MCPHttpServer**: HTTP server with SSE endpoint for event streaming
- **MethodDispatcher**: Routes JSON-RPC calls to appropriate handlers
- **Business Managers**: DebugController, MemoryManager, RegisterManager, etc.
- **Event System**: Real-time debugging event notifications via SSE

## Security Considerations

- By default, memory and register write operations are **disabled**
- Enable write permissions in `config.json` only if needed
- Server listens on localhost (127.0.0.1) by default
- Single client connection limit prevents resource exhaustion
- All operations require the debugger to be in a paused state

## Troubleshooting

### Plugin not loading
- Ensure the plugin file is in the correct directory
- Check x64dbg log for error messages
- Verify x64dbg version compatibility (requires x64dbg build 2023+)

### Server won't start
- Check if port 3000 is already in use
- Verify config.json is valid JSON
- Check file permissions on the plugin directory
- Review x64dbg log file for detailed error messages

### Connection refused
- Ensure HTTP server is started via plugin menu ("Start MCP HTTP Server")
- Check firewall settings for port 3000
- Verify client is connecting to http://127.0.0.1:3000
- Try accessing http://127.0.0.1:3000 in a web browser to test

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes with clear commit messages
4. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- [x64dbg](https://x64dbg.com/) - The debugger this plugin extends
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library
- Model Context Protocol specification

## Contact

- GitHub Issues: For bug reports and feature requests

---

**Note**: This is experimental software. Use at your own risk. Always test in a safe environment before using with critical applications.
