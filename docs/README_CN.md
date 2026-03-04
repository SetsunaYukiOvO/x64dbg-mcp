# x64dbg MCP 服务器插件

[English](../README.md) | 中文

这是一个面向 x64dbg 与 x32dbg 的 Model Context Protocol (MCP) 服务器实现，通过 JSON-RPC 2.0 接口提供远程调试能力。该插件允许外部应用与 AI 代理以编程方式和调试器交互。

**现已同时支持 x64 和 x86 架构！**

## 功能特性

- **完整 MCP 规范支持**：实现 MCP 三大核心构件
  - **Tools（69）**：可由 AI 调用的调试函数
  - **Resources（15）**：由应用控制的上下文数据源
  - **Prompts（10）**：用户引导的调试工作流模板

- **JSON-RPC 2.0 协议**：标准、语言无关的接口
- **HTTP + SSE 通信**：通过 Server-Sent Events 提供现代 Web 集成

- **Tools - AI 可控调试（69 个函数）**：
  - 执行控制（run、pause、step、run_to）
  - 内存读/写/搜索/分配
  - 寄存器访问（50+ 寄存器，含 GPR、SSE、AVX）
  - 断点管理（软件断点、硬件断点、内存断点、条件断点、日志断点）
  - 反汇编与符号解析
  - 线程管理（列出、切换、挂起、恢复）
  - 调用栈追踪与分析
  - **Dump 与脱壳**（模块 dump、内存 dump、自动脱壳、OEP 检测、IAT 重建）
  - **脚本执行**（执行 x64dbg 命令、批量操作）
  - **上下文快照**（捕获并比较调试状态）

- **Resources - 上下文提供器（15 个来源）**：
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

## v1.0.3 更新内容

- **通用化脱壳逻辑**
  - 扩展转移/OEP 识别模式（`E9`、`EB`、`FF25`、`push-ret`、`mov-jmp`、`movabs-jmp`）
  - 将 packed 检测重构为通用布局启发式评分模型
  - 移除 `UPX2` 硬编码导入回退路径

- **Dump/脱壳稳定性修复**
  - 修复 `dump_auto_unpack` 假成功场景（可能返回复制的壳层镜像）
  - 修复导入回退破坏节区原始布局并导致 dumped EXE 崩溃的问题
  - 提升 `debug_pause` 可靠性（强制中断 + 暂停状态确认）
  - 修复 `dump_auto_unpack` 默认 `max_iterations` 不一致问题（`tools/list` 为 `10`，运行时曾为 `3`），现统一为 `10`

- **运行态恢复能力**
  - 为 `dump_module`、`dump_analyze_module`、`dump_detect_oep` 增加自动暂停状态恢复
  - 为 `dump_auto_unpack` 增加运行中且上下文位于目标模块外时的执行上下文恢复
  - 提升运行态调用路径下自动脱壳可靠性

## 历史版本

### v1.0.2

- 自动化测试关键缺陷修复
- 构建系统改进与双架构统一输出
- 文档清理

### v1.0.1

- 线程与栈管理 API
- 增强错误处理与日志

完整版本历史见 [CHANGELOG_CN.md](../CHANGELOG_CN.md)

## 从源码构建

### 前置要求

- **Windows 10/11**（x64）
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
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DXDBG_ARCH=x64

# x86 构建
cmake -B build -G "Visual Studio 17 2022" -A Win32 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DXDBG_ARCH=x86
```

4. **构建**：
```powershell
cmake --build build --config Release
```

5. **输出**：
- 插件文件：`build\bin\Release\x64dbg_mcp.dp64`（约 611 KB）

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

1. 打开 x64dbg
2. 进入 **Plugins -> MCP Server -> Start MCP HTTP Server**
3. 服务器会在配置端口启动（默认：3000）
4. 访问 `http://127.0.0.1:3000`

### 配置

编辑 `config.json` 自定义设置：

```json
{
  "version": "1.0.3",
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

# 订阅调试事件
for event in client.subscribe_events():
    print(f"Event: {event}")
```

### VS Code 集成

在 VS Code 设置或 MCP 客户端配置中添加：

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
- `stack.get_pointers` - 获取栈指针（RSP、RBP）
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
- 确认插件文件位于正确目录
- 检查 x64dbg 日志中的错误信息
- 验证 x64dbg 版本兼容性（要求 x64dbg build 2023+）

### 服务器无法启动
- 检查端口 3000 是否已被占用
- 验证 config.json 是否为合法 JSON
- 检查插件目录的文件权限
- 查看 x64dbg 日志获取详细错误

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
