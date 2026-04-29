# 快速入门指南

[English](../QUICKSTART.md) | 中文

几分钟内开始使用 x64dbg MCP 服务器。

## 前置要求

- Windows 10/11 (x64 或 x86)
- 已安装 x64dbg 或 x32dbg 调试器
- Visual Studio 2022 with C++ Desktop Development 工作负载
- CMake 3.15+
- vcpkg 包管理器

## 安装

### 方式一：使用预编译版本（推荐）

1. 从 [GitHub Releases](https://github.com/SetsunaYukiOvO/x64dbg-mcp/releases) 下载最新版本
2. 选择适合的版本：
   - `x64dbg_mcp.dp64` 用于 x64dbg（64位）
   - `x32dbg_mcp.dp32` 用于 x32dbg（32位）
3. 解压到调试器的插件目录：
   - x64dbg: `x64dbg\x64\plugins\`
   - x32dbg: `x64dbg\x32\plugins\`
4. 复制 `config.json` 到 `plugins/x64dbg-mcp/`（或 `plugins/x32dbg-mcp/`）
5. 重启调试器

### 方式二：从源码构建

#### 1. 安装 vcpkg

```powershell
# 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg

# 引导 vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# 设置环境变量（可选）
setx VCPKG_ROOT "C:\vcpkg"
```

#### 2. 构建插件

```powershell
# 克隆仓库
git clone https://github.com/SetsunaYukiOvO/x64dbg-mcp.git
cd x64dbg-mcp

# 构建 x64 版本（默认）
.\build.bat

# 构建 x86 版本（32位）
.\build.bat --arch x86

# 或使用特定选项构建
.\build.bat --clean          # 清理构建
.\build.bat --arch x86 --clean  # 清理 x86 构建
.\build.bat --debug          # 调试构建
.\build.bat --help           # 显示所有选项
```

构建脚本将：
- 自动检测 vcpkg 安装
- 下载并编译依赖项（nlohmann_json）
- 使用 Visual Studio 为所选架构构建插件
- 可选择自动安装到调试器插件目录

**输出文件：**
- x64: `build\bin\Release\x64dbg_mcp.dp64`
- x86: `build\bin\Release\x32dbg_mcp.dp32`

#### 3. 手动构建（高级）

```powershell
# x64 配置
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DXDBG_ARCH=x64

# 或 x86 配置
cmake -B build -G "Visual Studio 17 2022" -A Win32 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DXDBG_ARCH=x86

# 编译
cmake --build build --config Release

# 输出：
# - x64: build\bin\Release\x64dbg_mcp.dp64
# - x86: build\bin\Release\x32dbg_mcp.dp32
```

## 1. 安装插件

如果你没有使用构建脚本的自动安装：

```powershell
# x64dbg
copy build\bin\Release\x64dbg_mcp.dp64 C:\x64dbg\x64\plugins\

# x32dbg
copy build\bin\Release\x32dbg_mcp.dp32 C:\x64dbg\x32\plugins\

# 复制配置文件（根据架构调整路径）
mkdir C:\x64dbg\x64\plugins\x64dbg-mcp
copy config.json C:\x64dbg\x64\plugins\x64dbg-mcp\

# 或 x32
mkdir C:\x64dbg\x32\plugins\x32dbg-mcp
copy config.json C:\x64dbg\x32\plugins\x32dbg-mcp\
```

## 2. 启动服务器

1. 启动 x64dbg 或 x32dbg
2. 加载目标程序进行调试
3. 菜单：**插件 → MCP Server → Start MCP HTTP Server**
4. 服务器在端口 3000 上启动（可在 config.json 中配置）
5. 在浏览器中访问 http://127.0.0.1:3000 验证服务器运行

## 3. 连接客户端

### Python 示例

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

# 使用客户端
client = MCPClient()
print(client.call("initialize"))
print(client.call("tools/list"))
```

### MCP 协议示例

```python
# 初始化 MCP 会话
init_response = client.call("initialize", {
    "protocolVersion": "2024-11-05",
    "capabilities": {},
    "clientInfo": {
        "name": "my-client",
        "version": "1.0.0"
    }
})

# 发送 initialized 通知
client.call("notifications/initialized")

# 列出可用工具、资源和提示词
tools = client.call("tools/list")
resources = client.call("resources/list")
resource_templates = client.call("resources/templates/list")
prompts = client.call("prompts/list")
print(tools)
print(resources)
print(resource_templates)
print(prompts)
```

说明：Cursor 等 MCP 客户端通常会根据 `initialize` 响应里的 capabilities 来显示 Tools、Resources、Prompts 分类。如果你刚升级插件但界面里仍未显示新分类，请重载窗口或重新添加该 MCP server 以刷新握手结果。

## 常用操作

### 获取系统信息
```python
response = client.call("system.info")
```

### 读取寄存器
```python
response = client.call("register.get", {"name": "rax"})
value = response["result"]["value"]
```

### 读取内存
```python
response = client.call("memory.read", {
    "address": "0x140001000",
    "size": 100
})
data = response["result"]["data"]  # 十六进制字符串
```

### 设置断点
```python
response = client.call("breakpoint.set", {
    "address": "0x140001000",
    "type": "software"
})
```

### 反汇编
```python
response = client.call("disassembly.at", {
    "address": "0x140001000",
    "count": 10
})
instructions = response["result"]["instructions"]
```

## 配置

编辑 `config.json` 进行自定义：

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

## 下一步

- 查看 [README_CN.md](README_CN.md) 获取完整的 API 参考
- 使用 `system.methods` API 调用来发现所有可用方法
- 探索 [examples/](../examples/) 查看更多客户端实现

## 故障排除

### 构建问题

**CMake 找不到 vcpkg**
- 确保设置了 `VCPKG_ROOT` 环境变量
- 或在 CMAKE_TOOLCHAIN_FILE 中使用完整路径
- 默认位置：`C:\vcpkg`

**构建时出现链接错误**
- 确保 x64dbg SDK 库文件存在于 `include/x64dbg-pluginsdk/`
- 尝试清理重建：`.\build.bat --clean`
- 验证是否为 x64 架构构建

**找不到 vcpkg**
- 安装 vcpkg：`git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg`
- 引导：`C:\vcpkg\bootstrap-vcpkg.bat`

### 运行时问题

**插件无法加载**
- 确保插件文件名为 `x64dbg_mcp.dp64`
- 检查 x64dbg 版本（需要 64 位版本）
- 查看 x64dbg 日志了解错误信息

**服务器无法启动**
- 检查端口 3000 是否未被占用
- 验证 config.json 是否为有效的 JSON
- 确保在 x64dbg 中加载了程序
- 查看 x64dbg 日志获取详细错误信息

**连接被拒绝**
- 确保通过插件菜单启动了 HTTP 服务器（"Start MCP HTTP Server"）
- 检查防火墙设置，允许端口 3000
- 验证客户端连接到 http://127.0.0.1:3000
- 在浏览器中测试：http://127.0.0.1:3000

## 构建脚本选项

`build.bat` 脚本支持以下选项：

```powershell
build.bat [选项]

选项:
  --debug         构建 Debug 模式（默认：Release）
  --clean         构建前清理构建目录
  --help          显示帮助信息

示例:
  build.bat                    # Release 构建
  build.bat --debug            # Debug 构建
  build.bat --clean            # 清理并重建
  build.bat --clean --debug    # 清理 Debug 构建
```

## 开发技巧

### 快速重建周期

```powershell
# 修改代码...

# 重建（更快，增量编译）
.\build.bat

# x64dbg 必须重启才能重新加载插件
```

### 开发用的 Debug 构建

```powershell
# 构建带调试符号的版本
.\build.bat --debug

# 调试输出：build\bin\Debug\x64dbg_mcp.dp64
```

## 高级功能（v1.1.0+）

### 脚本执行

以编程方式执行 x64dbg 命令：

```python
# 执行单个命令
response = client.send_request("script.execute", {
    "command": "bp MessageBoxA"
})

# 批量执行命令
response = client.send_request("script.execute_batch", {
    "commands": [
        "log \"开始分析...\"",
        "bp kernel32.CreateFileW",
        "bp kernel32.WriteFile",
        "run"
    ],
    "stop_on_error": True  # 如果任何命令失败则停止
})

# 获取最后一次命令执行结果
response = client.send_request("script.get_last_result")
```

### 上下文快照

捕获和比较调试状态：

```python
# 捕获初始状态
snapshot1 = client.send_request("context.get_snapshot", {
    "include_stack": True,
    "include_threads": True,
    "include_modules": True,
    "include_breakpoints": True
})

# 执行一些步骤
client.send_request("debug.step_over")

# 捕获新状态
snapshot2 = client.send_request("context.get_snapshot", {
    "include_stack": True,
    "include_threads": False,  # 跳过线程以加快捕获速度
    "include_modules": False,
    "include_breakpoints": False
})

# 比较快照查看变化
diff = client.send_request("context.compare_snapshots", {
    "snapshot1": snapshot1["result"],
    "snapshot2": snapshot2["result"]
})

print("检测到变化:", diff["result"]["has_differences"])
print("寄存器变化:", diff["result"]["differences"].get("registers", []))
```

### 快速上下文检查

```python
# 获取基础上下文（仅寄存器 + 状态）
context = client.send_request("context.get_basic")
print("寄存器:", context["result"]["registers"])
print("正在调试:", context["result"]["state"]["is_debugging"])
```

### 自动化分析工作流

```python
# 1. 设置环境
client.send_request("script.execute_batch", {
    "commands": [
        "bp VirtualAlloc",
        "bp VirtualProtect",
        "run"
    ]
})

# 2. 在断点处捕获状态
bp_snapshot = client.send_request("context.get_snapshot", {
    "include_stack": True
})

# 3. 使用脚本分析
client.send_request("script.execute_batch", {
    "commands": [
        "log \"VirtualAlloc 被调用！\"",
        "? rcx",
        "? rdx"
    ]
})

# 4. 继续并比较
client.send_request("debug.run")
after = client.send_request("context.get_snapshot")
diff = client.send_request("context.compare_snapshots", {
    "snapshot1": bp_snapshot["result"],
    "snapshot2": after["result"]
})
```

### 示例脚本

查看 `examples/` 目录：

- `python_client_http.py` - 基础 HTTP 客户端
- `advanced_features_demo.py` - v1.1.0+ 功能演示

运行演示：

```powershell
cd examples
python advanced_features_demo.py
```

## 下一步
