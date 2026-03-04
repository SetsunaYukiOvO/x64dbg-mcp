# 更新日志

x64dbg MCP Server Plugin 的所有重要变更都会记录在此文件中。

格式遵循 [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)，
并采用 [Semantic Versioning](https://semver.org/spec/v2.0.0.html)。

## [1.0.3] - 2026-03-04

### 修复
- **Dump/脱壳稳定性与回归修复**
  - 修复 `dump_auto_unpack` 的假成功行为：可能仅复制壳层镜像而未真正脱壳
  - 修复导入表回退流程破坏 dump 节区原始布局（`PointerToRawData`/`SizeOfRawData`）并导致运行崩溃的问题
  - 修复 `debug_pause` 在部分场景下返回成功但未真正中断目标进程的问题
  - 修复 `dump_auto_unpack` 默认迭代次数不一致：
    - `tools/list` 暴露 `max_iterations=10`，而运行时回退值为 `3`
    - 运行时默认值现已统一为 `10`
  - 修复 dump 工具在运行态下的行为：
    - `dump_module` 不再仅因 `Debugger is not paused` 而直接失败，现会尝试恢复
    - `dump_analyze_module` 不再因依赖暂停状态而误报 `is_packed=false`
    - `dump_detect_oep` 在目标运行时调用不再失败或误报
  - 修复 `dump_auto_unpack` 在执行上下文位于目标模块范围外时调用失败的问题
- **线程切换可靠性与状态一致性**
  - 修复 `thread.switch` 使用十进制 TID 文本（例如 `8164`）时，可能被 x64dbg 命令解析器当作十六进制表达式的问题
  - `thread.switch`/`thread.suspend`/`thread.resume` 现统一向命令层发送显式 `0xHEX` 线程 ID
  - 在 `ThreadManager::SwitchThread` 中增加切换后验证循环，仅当 `GetCurrentThreadId()==target` 时返回成功
  - 修复 `thread.switch` 响应字段不一致：
    - 新增 `requested_id`
    - `current_id` 现在返回切换后的真实当前线程 ID，而不是仅回显请求值

### 变更
- **通用化脱壳逻辑（不再采用 UPX 专用流程）**
  - 通用化脱壳跳转/OEP 模式识别：
    - `E9 rel32`、`EB rel8`、`FF 25` 间接跳转
    - `push imm32; ret`
    - `mov reg, imm; jmp reg`（x86/x64）
    - `movabs reg, imm64; jmp reg`（x64）
  - 将 packed 检测重构为布局/启发式评分模型，不再依赖 UPX 特化分支
  - 移除 `UPX2` 硬编码导入回退，替换为通用导入相关节区启发式
  - 为 dump/analyze/OEP 检测入口增加自动暂停状态恢复
  - 为 `dump_auto_unpack` 增加运行中且上下文位于目标模块外时的模块上下文恢复路径

## [1.0.2] - 2025-12-16

### 修复
- **自动化测试发现的关键缺陷修复**
  - 修复 `breakpoint_toggle` 返回启用状态不一致的问题
  - 为 `memory_search` 实现真实搜索功能（此前为占位实现）
  - 修复 `memory_get_info`：使用 VirtualQueryEx API 返回正确的内存区域基址
  - 修复 `debug_step_over` 与 x64dbg 状态更新的 RIP 同步时序问题
  - 增强 `dump_detect_oep` 策略校验并提供清晰错误信息
  - 为多个工具补齐缺失的诊断字段：
    - `script_get_last_result`：增加 `error` 字段
    - `stack_read_frame`：增加 `encoding` 字段
    - `dump_memory_region`：增加完整 `progress` 信息

### 变更
- **构建系统改进**
  - 新增双架构构建脚本：单条命令同时编译 x64 与 x86
  - 统一双架构输出目录（`dist/`）
  - 使用 `-j` 并行编译以加快构建速度
  - 简化构建选项：`--clean`、`--x64-only`、`--x86-only`
  - 构建脚本不再因交互提示阻塞

### 移除
- **文档清理**
  - 移除冗余技术文档文件
  - 精简为核心文档集合

## [1.1.0] - 2025-12-XX

### 新增

#### 双架构支持
- **多架构构建系统**：插件现同时支持 x64 与 x86
  - 使用 `.\build.bat` 或 `.\build.bat --arch x64` 构建 x64dbg（64 位）版本
  - 使用 `.\build.bat --arch x86` 构建 x32dbg（32 位）版本
  - 输出文件分离：`x64dbg_mcp.dp64` 与 `x32dbg_mcp.dp32`
- **架构感知寄存器处理**
  - x64：RAX、RBX、RCX、RDX、RSI、RDI、RSP、RBP、RIP、R8-R15
  - x86：EAX、EBX、ECX、EDX、ESI、EDI、ESP、EBP、EIP
- **统一 SDK 结构**：双架构共享 `include/x64dbg-pluginsdk` 头文件

#### Dump 与脱壳模块
- **完整的内存 Dump 与自动脱壳能力**
  - `dump.module`：导出可执行模块并自动重建 PE
    - 导入表修复（IAT 重建）
    - 重定位表处理
    - 入口点（OEP）修复
    - PE 头重建
    - 节区对齐
  - `dump.memory_region`：导出任意内存区域到文件
  - `dump.auto_unpack`：自动脱壳并检测 OEP
    - 支持多层壳
    - 支持可配置最大迭代次数的迭代脱壳
    - 自动壳类型检测
  - `dump.analyze_module`：检测壳并分析模块结构
    - UPX 检测
    - ASPack 检测
    - 基于启发式的通用壳检测
  - `dump.detect_oep`：多策略 OEP 检测
    - 基于熵的检测
    - 基于模式的检测（函数序言扫描）
    - 基于执行轨迹的检测
    - 自定义 AI 驱动检测策略
  - `dump.get_dumpable_regions`：枚举所有可导出内存区域
  - `dump.fix_imports`：独立导入表重建
    - 标准 IAT 修复
    - Scylla 风格高级 IAT 重建
  - `dump.rebuild_pe`：独立 PE 头重建
- **DumpManager**：Dump 操作核心业务逻辑
- **DumpHandler**：Dump 方法的 JSON-RPC 处理器

#### 脚本执行 API
- **以编程方式执行 x64dbg 命令**
  - `script.execute`：执行单条 x64dbg 命令
  - `script.execute_batch`：批量执行命令并带错误处理
  - `script.get_last_result`：获取最近一次命令执行结果

#### 上下文快照 API
- **捕获与比较调试状态**
  - `context.get_snapshot`：完整调试上下文快照
  - `context.get_basic`：快速寄存器 + 状态快照
  - `context.compare_snapshots`：比较两个快照并找出差异

### 功能
- 通过回调系统实现 AI 可定制脱壳策略
- 为长耗时 Dump 操作提供进度回调
- 自动 PE 结构校验与修复
- 支持原始二进制 dump 与 PE 修复后 dump
- 支持批量区域导出
- 与现有断点与调试控制系统集成
- 使用 `XDBG_ARCH_X64` 与 `XDBG_ARCH_X86` 宏进行条件编译
- 栈操作使用架构相关指针大小（x64 为 8 字节，x86 为 4 字节）
- 线程上下文获取适配 32 位与 64 位环境

### 技术
- 新增 `DumpManager` 统一管理 Dump 操作
- PE 格式处理工具
- 内存区域枚举与校验
- 壳特征数据库（可扩展）
- OEP 检测算法集合
- IAT 重建引擎
- CMake 通过 `XDBG_ARCH` 变量选择目标架构
- 构建脚本（`build.bat`、`configure.bat`、`build.sh`）支持 `--arch` 参数
- 将所有 `X64DBG_SDK_AVAILABLE` 宏替换为 `XDBG_SDK_AVAILABLE`
- 新增 `duint` 类型定义：x64 为 `uint64_t`，x86 为 `uint32_t`
- 为架构兼容改造 `RegisterManager`、`StackManager`、`ThreadManager`
- CMake 根据架构链接正确 SDK 库：
  - x64：`x64bridge.lib`、`x64dbg.lib`
  - x86：`x32bridge.lib`、`x32dbg.lib`

### 文档
- 新增 `UPDATE.md` 与 `UPDATE_CN.md` 用于版本特性摘要
- 更新 `docs/Protocol.md`，新增 14 个方法（8 个 dump.*、3 个 script.*、3 个 context.*）
- 更新 `README.md`，突出所有新增能力
- 新增 dump、script、context API 的完整使用示例
- 新增全面的 dump 演示脚本（`examples/dump_demo.py`）

### 安全
- Dump 操作需要写权限
- 输出路径校验
- 遵循内存保护属性
- 通过大小限制避免资源耗尽

## [1.0.0] - 2025-11-20
## [1.0.0] - 2025-11-18

### 新增
- 首次发布，包含完整核心功能
- JSON-RPC 2.0 协议实现
- 带 SSE 事件流的 HTTP 服务器
- 覆盖多个分类的 50+ 调试方法：
  - **调试控制**：run、pause、step_into、step_over、step_out、run_to、restart、stop、get_state
  - **内存操作**：read、write、search、get_info、enumerate、allocate、free、get_protection
  - **寄存器操作**：get、set、list、get_batch（50+ 寄存器）
  - **断点管理**：set、delete、enable、disable、toggle、list、get、delete_all、set_condition、set_log
  - **反汇编**：at、range、function
  - **符号解析**：resolve、from_address、search、list、modules
  - **注释/标签管理**：set_comment、get_comment、set_label
  - **模块信息**：list、get、get_main、find
  - **线程管理**：list、switch、get_current、suspend、resume
  - **栈操作**：get_trace、read_frame、get_pointers、is_on_stack
- 基于权限的访问控制系统
- 配置文件支持（config.json）
- 多级日志系统
- 通过 SSE 的事件通知系统
- 带 JSON-RPC 错误码的完整错误处理
- Python 客户端示例

### 技术细节
- 基于 C++17 构建
- 使用 x64dbg Plugin SDK
- 依赖：通过 vcpkg 获取 nlohmann/json
- 支持 Windows x64 平台上的 x64dbg
- 插件体积：约 445 KB

---

## 版本历史摘要

| 版本 | 发布日期 | 关键特性 |
|------|----------|----------|
| 1.0.3 | 2026-03-04 | 通用化脱壳流程、运行态 dump 恢复、自动脱壳稳定性修复、线程切换一致性修复 |
| 1.1.0 | TBD | 双架构支持、Dump 与脱壳、脚本执行、上下文快照 |
| 1.0.0 | 2025-11-18 | 首次公开发布 |

---

## 未来路线图

### 计划在 v1.2.0
- 性能优化与基准测试
- 增强安全能力（Token/API Key 认证）
- WebSocket 协议支持
- 多客户端连接支持

### 计划在 v2.0.0
- AI 辅助分析功能
- 自动模式识别
- 调用图生成
- CFG（控制流图）导出
- 高级内存可视化

---

## 升级说明

### 从 1.0.0 升级到 1.1.0
- **破坏性变更**：无
- **新增特性**：
  - 双架构支持（x64/x86）
  - 14 个新方法（8 个 dump.*、3 个 script.*、3 个 context.*）
  - AI 驱动脱壳能力
- **配置更新**：
  - 如果启用方法白名单，请在 `config.json` 的 `allowed_methods` 中加入 `dump.*`、`script.*`、`context.*`
  - 对于 x86 构建，请使用 `x32dbg_mcp.dp32`（而非 `x64dbg_mcp.dp64`）
- **向后兼容性**：现有客户端代码无需修改即可继续工作

---

更多信息：
- [README.md](README.md) - 主文档
- [UPDATE.md](UPDATE.md) - 版本更新摘要
- [QUICKSTART.md](QUICKSTART.md) - 快速开始
- [Plan.md](Plan.md) - 开发路线图
- [examples/](examples/) - 代码示例
