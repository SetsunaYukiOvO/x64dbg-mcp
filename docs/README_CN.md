# x64dbg MCP 服务器插件

[English](../README.md) | 中文

这是一个面向 x64dbg 与 x32dbg 的 Model Context Protocol (MCP) 服务器实现，通过 JSON-RPC 2.0 接口提供远程调试能力。该插件允许外部应用与 AI 代理以编程方式和调试器交互。

**现已同时支持 x64 和 x86 架构！**

## 功能特性

- **完整 MCP 规范支持**：实现 MCP 三大核心构件
  - **Tools（79）**：可由 AI 调用的调试函数
  - **Resources（7 + 8 模板）**：由应用控制的上下文数据源
  - **Prompts（10）**：用户引导的调试工作流模板

- **JSON-RPC 2.0 协议**：标准、语言无关的接口
- **Streamable HTTP 传输**（MCP 2025-03-26 规范）端点 `/mcp`，并保留旧版 HTTP+SSE 端点 `/sse` 兼容老客户端

- **Tools - AI 可控调试（79 个函数）**：
  - 执行控制（init/run/pause/step/run_to/restart/stop）
  - 内存读/写/搜索/分配
  - 寄存器访问（50+ 寄存器，含 GPR、SSE、AVX）
  - 断点管理（软件断点、硬件断点、内存断点、条件断点、日志断点）
  - 反汇编与符号解析
  - 线程管理（列出、切换、挂起、恢复）
  - 调用栈追踪与分析
  - **Dump 与分析**（模块 dump、内存 dump、加壳检测、OEP 检测）
  - **脚本执行**（执行 x64dbg/x32dbg 命令、批量操作）
  - **上下文快照**（捕获并比较调试状态）

- **Resources - 上下文提供器（7 个直接资源 + 8 个模板）**：
  - 直接资源：调试器状态、寄存器、模块、线程、内存映射、断点、栈
  - 资源模板：内存内容、反汇编、模块信息、符号解析、函数分析
  - 只读、由应用控制的访问方式

- **Prompts - 工作流模板（10 个提示）**：
  - 崩溃分析、漏洞挖掘、函数追踪
  - 二进制脱壳、算法逆向、执行对比
  - 字符串狩猎、代码补丁、API 监控
  - 调试会话初始化

- **安全性**：基于权限的访问控制
- **可扩展性**：支持自定义方法、资源与提示的插件架构

## 从源码构建

### 前置要求

- **Windows 10/11**（64 位主机，可构建 x64 与 x86 插件）
- **CMake** 3.15 或更高
- **Visual Studio 2022**（安装 C++ 桌面开发工作负载）
- **vcpkg**（C++ 依赖包管理器）
- **Git**（用于克隆仓库）

### 快速构建

最简单的方式是使用仓库提供的构建脚本：

```powershell
# 克隆仓库
git clone https://github.com/SetsunaYukiOvO/x64dbg-mcp.git
cd x64dbg-mcp

# 同时构建 x64 和 x86（推荐）
.\build.bat

# 仅构建 x64
.\build.bat --x64-only

# 仅构建 x86
.\build.bat --x86-only

# 清理后重建
.\build.bat --clean

# 脚本将自动：
# 1. 检测 vcpkg 安装
# 2. 下载依赖（nlohmann_json）
# 3. 为双架构配置 CMake
# 4. 使用 Visual Studio 并行编译
# 5. 将输出文件复制到 dist/ 目录
```

构建脚本选项：
```powershell
.\build.bat               # 构建 x64 + x86（Release）
.\build.bat --clean       # 清理并重建双架构
.\build.bat --x64-only    # 仅构建 x64
.\build.bat --x86-only    # 仅构建 x86
.\build.bat --debug       # Debug 构建（未来支持）
```

**输出文件**（位于 `dist/` 目录）：
- x64 插件：`dist\x64dbg_mcp.dp64`（约 837 KB）
- x86 插件：`dist\x32dbg_mcp.dp32`（约 800 KB）

### 手动构建步骤

如果你希望手动控制流程：

1. **安装 vcpkg**（如未安装）：
```powershell
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT "C:\vcpkg"
```

2. **克隆仓库**：
```powershell
git clone https://github.com/SetsunaYukiOvO/x64dbg-mcp.git
cd x64dbg-mcp
```

3. **配置 CMake**：
```powershell
# x64 构建
cmake -B build_x64 -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DXDBG_ARCH=x64

# x86 构建
cmake -B build_x86 -G "Visual Studio 17 2022" -A Win32 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x86-windows ^
    -DXDBG_ARCH=x86
```

4. **构建**：
```powershell
cmake --build build_x64 --config Release
cmake --build build_x86 --config Release
```

5. **输出**：
- x64 插件：`build_x64\bin\Release\x64dbg_mcp.dp64`
- x86 插件：`build_x86\bin\Release\x32dbg_mcp.dp32`

## 安装

1. 将编译好的插件复制到对应调试器目录：

```powershell
# x64dbg（64 位）
# 将 <x64dbg-path> 替换为你的实际安装目录
copy dist\x64dbg_mcp.dp64 <x64dbg-path>\x64\plugins\

# x32dbg（32 位）
copy dist\x32dbg_mcp.dp32 <x64dbg-path>\x32\plugins\

# 示例（安装在 C:\x64dbg）：
# copy dist\x64dbg_mcp.dp64 C:\x64dbg\x64\plugins\
# copy dist\x32dbg_mcp.dp32 C:\x64dbg\x32\plugins\
```

2. （可选）复制配置文件：
```powershell
# x64dbg
mkdir <x64dbg-path>\x64\plugins\x64dbg-mcp
copy config.json <x64dbg-path>\x64\plugins\x64dbg-mcp\

# x32dbg
mkdir <x64dbg-path>\x32\plugins\x32dbg-mcp
copy config.json <x64dbg-path>\x32\plugins\x32dbg-mcp\
```

3. 重启 x64dbg/x32dbg 以加载插件

## 使用方法

### 启动服务器

1. 64 位目标打开 x64dbg，32 位目标打开 x32dbg
2. 进入 **Plugins -> MCP Server -> Start MCP HTTP Server**
3. 服务器会在配置端口启动（默认：3000）
4. 访问 `http://127.0.0.1:3000`

### 配置

编辑 `config.json` 自定义设置：

```json
{
  "version": "1.0.11",
  "server": {
    "address": "127.0.0.1",
    "port": 3000
  },
  "permissions": {
    "allow_memory_write": false,
    "allow_register_write": false,
    "allow_script_execution": false,
    "allow_breakpoint_modification": true,
    "allowed_methods": ["debug.*", "memory.*"]
  },
  "security": {
    "origin_allowlist": [],
    "host_allowlist": [],
    "auth_enabled": false,
    "auth_token": ""
  },
  "logging": {
    "enabled": true,
    "level": "info",
    "file": "x64dbg_mcp.log",
    "max_file_size_mb": 10,
    "console_output": true
  },
  "timeout": {
    "request_timeout_ms": 30000,
    "step_timeout_ms": 10000,
    "memory_read_timeout_ms": 5000
  },
  "features": {
    "enable_notifications": true,
    "enable_heartbeat": true,
    "heartbeat_interval_seconds": 30,
    "enable_batch_requests": true,
    "auto_start_mcp_on_plugin_load": false
  }
}
```

#### 配置编辑器

在 x64dbg 菜单中选择 **Plugins > MCP Server > Edit Config**，即可编辑当前生效的配置文件，无需手工修改 JSON。编辑器按 Server、Security、Permissions、Runtime、Logging 分页，覆盖上述示例的所有运行时配置；`version` 与未来可能新增的未知字段会在保存时保留。

Origin 和 Host 白名单每行填写一项。启用 **Require Bearer token authentication** 时 Token 输入框必填且会掩码显示。非回环监听地址且未启用认证时，保存会要求再次确认。保存后重启 MCP HTTP Server 才会应用服务端配置。

### 外部访问与安全边界

服务默认监听 `127.0.0.1:3000`，这是有意的安全默认值。当前版本**没有**校验 HTTP `Authorization`、Bearer Token、API Key 或 OAuth；`security.origin_allowlist` 和 `security.host_allowlist` 只用于校验浏览器来源和 Host，不是登录密码。

本机使用时直接连接 `http://127.0.0.1:3000/mcp` 即可。局域网或虚拟机访问时，先将配置改为：

```json
{
  "server": { "address": "0.0.0.0", "port": 3000 },
  "security": {
    "origin_allowlist": ["http://192.168.1.50:3000"],
    "host_allowlist": ["192.168.1.20", "debugger.example.test"]
  }
}
```

请替换为实际客户端 Origin、服务端 IP 或域名，不要使用 Origin `*`。还必须单独配置 Windows 防火墙、虚拟机 NAT/桥接网络以及路由器端口转发。修改后重启插件。

端点选择：

- 新版 Streamable HTTP：`http://HOST:3000/mcp`
- 旧版 HTTP+SSE：`http://HOST:3000/sse`
- JSON-RPC 兼容端点：`http://HOST:3000/rpc`
- 连通性检查：`GET http://HOST:3000/`

不要把插件端口直接暴露到公网。公网场景应使用 VPN、SSH 隧道或带 TLS、认证、限流和 IP 白名单的反向代理；代理与插件在同机时建议插件仍监听 `127.0.0.1`。代理必须支持 HTTP 流式响应，并转发 `Content-Type`、`Accept`、`Mcp-Protocol-Version`、`Mcp-Session-Id` 和 `Last-Event-ID`。

内置 Bearer 认证可通过以下配置启用，修改后重启插件：

```json
{
  "security": {
    "auth_enabled": true,
    "auth_token": "replace-with-a-long-random-secret"
  }
}
```

启用后，除 CORS `OPTIONS` 预检外的所有 HTTP 端点均要求 `Authorization: Bearer <auth_token>`。启用认证但 Token 为空时服务将拒绝启动；非回环地址且未启用认证时会记录安全告警。不要提交 Token，并应限制配置文件的访问权限。

内存写入、寄存器写入和脚本执行默认关闭。未认证的公网服务绝不能开启这些权限。

常见问题：

1. `Connection refused`：检查插件是否启动、监听地址/端口、虚拟机转发和防火墙。
2. 请求被拒绝：将浏览器实际 `Origin` 加入 `origin_allowlist`，将代理或客户端使用的 Host 加入 `host_allowlist`。
3. `initialize` 成功但没有工具：断开并重新连接，客户端根据 `initialize` 的 capabilities 发现工具。
4. SSE 404 或一直等待：旧客户端使用 `/sse`，新版客户端使用 `/mcp`。
5. 多层代理异常：检查 HTTP/1.1 流式传输、长连接超时和 SSE 缓冲设置。

排查时先在调试器主机执行 `curl http://127.0.0.1:3000/`，再逐层测试转发地址。日志中不要包含内存内容、Token 或凭据。

### 客户端示例

使用 HTTP 的 Python 客户端示例：

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
        """订阅 SSE 事件"""
        response = requests.get(
            f"{self.base_url}/sse",
            stream=True,
            headers={"Accept": "text/event-stream"}
        )
        for line in response.iter_lines():
            if line:
                yield line.decode('utf-8')

# 用法
client = MCPClient()
print(client.call("initialize"))
print(client.call("tools/list"))
print(client.call("resources/list"))
print(client.call("prompts/list"))

# 订阅调试事件
for event in client.subscribe_events():
    print(f"Event: {event}")
```

Cursor 以及其他 MCP 客户端通常会根据 `initialize` 返回的 capabilities 决定显示哪些分类。该服务端会声明 `tools`、`resources` 和 `prompts`，因此在重新连接后，客户端界面中应能看到这三类能力。

### VS Code 集成

在 VS Code 设置或 MCP 客户端配置中添加：

```json
{
  "mcpServers": {
    "x64dbg": {
      "type": "http",
      "url": "http://127.0.0.1:3000/mcp"
    }
  }
}
```

旧版 HTTP+SSE 客户端（注意是 `/sse` 路径，不是根路径）：

```json
{
  "mcpServers": {
    "x64dbg": {
      "type": "sse",
      "url": "http://127.0.0.1:3000/sse"
    }
  }
}
```

## 可用方法

### 系统方法
- `system.info` - 获取服务器信息
- `system.ping` - 测试连接
- `system.methods` - 列出所有可用方法

### 调试控制
- `debug.run` - 继续执行
- `debug.pause` - 暂停执行
- `debug.step_into` - 单步进入
- `debug.step_over` - 单步越过
- `debug.step_out` - 单步跳出函数
- `debug.get_state` - 获取当前调试状态
- `debug.run_to` - 运行到指定地址
- `debug.restart` - 重启调试会话
- `debug.stop` - 停止调试

### 寄存器操作
- `register.get` - 读取单个寄存器
- `register.set` - 写入寄存器值
- `register.list` - 列出所有寄存器
- `register.get_batch` - 批量读取寄存器

### 内存操作
- `memory.read` - 读取内存区域
- `memory.write` - 写入内存区域
- `memory.search` - 搜索内存模式
- `memory.get_info` - 获取内存区域信息
- `memory.enumerate` - 列出所有内存区域
- `memory.allocate` - 分配内存
- `memory.free` - 释放分配的内存

### 断点管理
- `breakpoint.set` - 设置断点
- `breakpoint.delete` - 删除断点
- `breakpoint.enable` - 启用断点
- `breakpoint.disable` - 禁用断点
- `breakpoint.toggle` - 切换断点状态
- `breakpoint.list` - 列出所有断点
- `breakpoint.get` - 获取断点详情
- `breakpoint.delete_all` - 删除所有断点
- `breakpoint.set_condition` - 设置断点条件
- `breakpoint.set_log` - 设置断点日志消息
- `breakpoint.reset_hitcount` - 重置断点命中计数

### 反汇编
- `disassembly.at` - 在指定地址反汇编
- `disassembly.range` - 反汇编地址范围
- `disassembly.function` - 反汇编整个函数

### 符号解析
- `symbol.resolve` - 将符号解析为地址
- `symbol.from_address` - 由地址获取符号
- `symbol.search` - 按模式搜索符号
- `symbol.list` - 列出所有符号
- `symbol.modules` - 列出已加载模块
- `symbol.set_label` - 设置符号标签
- `symbol.set_comment` - 设置符号注释
- `symbol.get_comment` - 获取符号注释

### 模块操作
- `module.list` - 列出所有已加载模块
- `module.get` - 获取模块信息
- `module.get_main` - 获取主模块

### 线程操作
- `thread.list` - 列出所有线程
- `thread.get_current` - 获取当前线程
- `thread.get` - 获取线程信息
- `thread.switch` - 切换到指定线程
- `thread.suspend` - 挂起线程
- `thread.resume` - 恢复线程
- `thread.get_count` - 获取线程数量

### 栈操作
- `stack.get_trace` - 获取调用栈
- `stack.read_frame` - 读取栈帧
- `stack.get_pointers` - 获取栈指针（x64 为 RSP/RBP，x86 为 ESP/EBP）
- `stack.is_on_stack` - 检查地址是否位于栈上

完整方法签名与示例请查看源码内联文档，或调用 `system.methods` API。

## 架构

该插件分为四层：

1. **通信层**：带 SSE 支持的 HTTP 服务，用于实时事件流
2. **协议层**：JSON-RPC 与 MCP 协议解析、校验与分发
3. **业务层**：调试操作、内存管理、符号解析
4. **插件层**：x64dbg 集成、事件处理、回调管理

### 核心组件

- **MCPHttpServer**：带 SSE 事件流端点的 HTTP 服务器
- **MethodDispatcher**：将 JSON-RPC 调用路由到对应处理器
- **Business Managers**：DebugController、MemoryManager、RegisterManager 等
- **Event System**：通过 SSE 推送实时调试事件通知

## 安全注意事项

- 默认情况下，内存和寄存器写操作为 **禁用**
- 仅在必要时于 `config.json` 中启用写权限
- 服务默认监听本地地址（127.0.0.1）
- 单客户端连接限制可避免资源耗尽
- 所有操作都要求调试器处于暂停状态

## 故障排查

### 插件未加载
- 确认插件与调试器匹配：x64dbg 使用 `x64\plugins\x64dbg_mcp.dp64`，x32dbg 使用 `x32\plugins\x32dbg_mcp.dp32`
- 不要在 x32dbg 中加载 `.dp64`，也不要在 x64dbg 中加载 `.dp32`
- 检查对应的 x64dbg/x32dbg 日志
- 验证调试器版本兼容性（要求 x64dbg/x32dbg build 2023+）

### 服务器无法启动
- 检查端口 3000 是否已被占用
- 验证 config.json 是否为合法 JSON
- 检查插件目录的文件权限
- 查看对应的 x64dbg/x32dbg 日志获取详细错误

### 连接被拒绝
- 确保已通过插件菜单启动 HTTP 服务（"Start MCP HTTP Server"）
- 检查防火墙对 3000 端口的设置
- 确认客户端连接地址为 `http://127.0.0.1:3000`
- 可在浏览器访问 `http://127.0.0.1:3000` 进行连通性测试

## 贡献

欢迎贡献。请按以下流程：
1. Fork 仓库
2. 创建功能分支
3. 进行修改并提交清晰的 commit 信息
4. 提交 Pull Request

## 许可证

本项目采用 MIT 许可证，详见 LICENSE 文件。

## 致谢

- [x64dbg](https://x64dbg.com/) - 本插件所扩展的调试器
- [nlohmann/json](https://github.com/nlohmann/json) - JSON 库
- Model Context Protocol 规范

## 联系方式

- GitHub Issues：用于缺陷反馈和功能请求

---

**注意**：这是实验性软件。请自行承担使用风险，在关键场景使用前务必先在安全环境中测试。
