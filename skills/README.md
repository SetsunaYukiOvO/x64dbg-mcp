# x64dbg Reverse Engineering Plugin for Claude Code

[English](#english) | [中文](#中文)

---

<a id="english"></a>

## Overview

A Claude Code plugin that provides 11 reverse engineering slash commands for x64dbg debugger via MCP protocol. Each command is a structured workflow that guides Claude through complex debugging and analysis tasks.

## Installation

### Method 1: Install from local path

```bash
# Clone the repository
git clone https://github.com/SetsunaYukiOvO/x64dbg-mcp.git

# Install the plugin (run in Claude Code)
/install-plugin path/to/x64dbg-mcp/skills
```

### Method 2: Copy to project

```bash
# Copy the entire plugin directory into your project
cp -r path/to/x64dbg-mcp/skills .claude/plugins/x64dbg-re
```

### Method 3: Development mode

```bash
claude --plugin-dir path/to/x64dbg-mcp/skills
```

## Prerequisites

1. **x64dbg** with the MCP server plugin installed and running
2. **Claude Code** with MCP server configured:

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

## Available Commands

| Command | Description |
|---------|-------------|
| `/debug-session [issue]` | Initialize debugging session with environment assessment |
| `/analyze-crash [address]` | Systematic crash root cause analysis |
| `/unpack [packer-hint]` | Automated unpacking for packed executables |
| `/trace-function <name>` | Function tracing with parameter monitoring |
| `/find-vuln [type]` | Security vulnerability scanning |
| `/reverse-algo <address>` | Algorithm identification and pseudocode |
| `/api-monitor [category]` | Windows API call logging setup |
| `/patch-code <goal>` | Guided binary patching with backup |
| `/hunt-strings [pattern]` | String search with cross-references |
| `/compare-state` | Capture and diff execution states |
| `/dump-memory [target]` | Memory dumping with PE reconstruction |

## Workflow Examples

### Malware Analysis

```
/debug-session suspicious.exe
/api-monitor process
/api-monitor network
/hunt-strings http
/unpack
```

### Vulnerability Research

```
/debug-session target.exe
/find-vuln buffer-overflow
/trace-function parse_input
/reverse-algo 0x401000
```

### CTF / Crackme

```
/debug-session crackme.exe
/hunt-strings
/trace-function check_serial
/reverse-algo 0x401234
/patch-code bypass check at 0x401234
```

---

<a id="中文"></a>

## 概述

一个 Claude Code 插件，提供 11 个逆向工程斜杠命令，通过 MCP 协议与 x64dbg 调试器交互。每个命令都是一个结构化工作流，指导 Claude 完成复杂的调试和分析任务。

## 安装

### 方式一：从本地路径安装

```bash
# 克隆仓库
git clone https://github.com/SetsunaYukiOvO/x64dbg-mcp.git

# 在 Claude Code 中安装插件
/install-plugin path/to/x64dbg-mcp/skills
```

### 方式二：复制到项目

```bash
# 将插件目录复制到项目中
cp -r path/to/x64dbg-mcp/skills .claude/plugins/x64dbg-re
```

### 方式三：开发模式

```bash
claude --plugin-dir path/to/x64dbg-mcp/skills
```

## 前提条件

1. 安装了 MCP 服务器插件的 **x64dbg**，且已启动
2. **Claude Code** 已配置 MCP 服务器：

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

## 可用命令

| 命令 | 描述 |
|------|------|
| `/debug-session [问题]` | 初始化调试会话，评估环境 |
| `/analyze-crash [地址]` | 系统化崩溃根因分析 |
| `/unpack [壳类型]` | 自动化脱壳 |
| `/trace-function <名称>` | 函数追踪，监控参数和返回值 |
| `/find-vuln [类型]` | 安全漏洞扫描 |
| `/reverse-algo <地址>` | 算法识别与伪代码生成 |
| `/api-monitor [类别]` | Windows API 调用日志 |
| `/patch-code <目标>` | 带备份的二进制补丁 |
| `/hunt-strings [模式]` | 字符串搜索与交叉引用 |
| `/compare-state` | 执行状态快照对比 |
| `/dump-memory [目标]` | 内存转储与 PE 重建 |

## 工作流示例

### 恶意软件分析
```
/debug-session suspicious.exe
/api-monitor process
/api-monitor network
/hunt-strings http
/unpack
```

### 漏洞研究
```
/debug-session target.exe
/find-vuln buffer-overflow
/trace-function parse_input
/reverse-algo 0x401000
```

### CTF / Crackme
```
/debug-session crackme.exe
/hunt-strings
/trace-function check_serial
/reverse-algo 0x401234
/patch-code bypass check at 0x401234
```
