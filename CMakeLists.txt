cmake_minimum_required(VERSION 3.15)
project(x64dbg_mcp VERSION 1.0.6 LANGUAGES CXX RC)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 选项
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_LOGGING "Enable logging" ON)

# 支持目标: x64 或 x86(x32dbg)
set(XDBG_ARCH "x64" CACHE STRING "Target xdbg architecture: x64 or x86")
set_property(CACHE XDBG_ARCH PROPERTY STRINGS x64 x86)

# 允许用户通过 -DXDBG_SDK_DIR 指定 SDK 路径；否则使用项目内置目录（按架构）
if(NOT DEFINED XDBG_SDK_DIR)
    # x64dbg 和 x32dbg 的 SDK 头文件是兼容的，共用同一个目录
    set(XDBG_BUNDLED_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/x64dbg-pluginsdk")
    
    # 如果未来需要分离 SDK，取消下面的注释：
    # if(XDBG_ARCH STREQUAL "x64")
    #     set(XDBG_BUNDLED_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/x64dbg-pluginsdk")
    # else()
    #     set(XDBG_BUNDLED_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/x32dbg-pluginsdk")
    # endif()
else()
    set(XDBG_BUNDLED_SDK_DIR "${XDBG_SDK_DIR}")
endif()

# 检查内置 SDK 是否存在
if(EXISTS "${XDBG_BUNDLED_SDK_DIR}/_plugins.h")
    message(STATUS "Using bundled xdbg SDK from: ${XDBG_BUNDLED_SDK_DIR}")
    set(SDK_AVAILABLE TRUE)
else()
    message(FATAL_ERROR "Bundled xdbg SDK not found at: ${XDBG_BUNDLED_SDK_DIR} (set XDBG_SDK_DIR to override)")
endif()

# 第三方库 - 使用 vcpkg 引入 nlohmann_json
find_package(nlohmann_json CONFIG REQUIRED)

# 源文件
set(CORE_SOURCES
    src/core/Exceptions.cpp
    src/core/Logger.cpp
    src/core/ConfigManager.cpp
    src/core/JSONRPCParser.cpp
    src/core/ResponseBuilder.cpp
    src/core/RequestValidator.cpp
    src/core/MethodDispatcher.cpp
    src/core/PermissionChecker.cpp
    src/core/MCPToolRegistry.cpp
    src/core/MCPResourceRegistry.cpp
    src/core/MCPPromptRegistry.cpp
)

set(COMMUNICATION_SOURCES
    src/communication/MCPHttpServer.cpp
)

set(BUSINESS_SOURCES
    src/business/DebugController.cpp
    src/business/RegisterManager.cpp
    src/business/MemoryManager.cpp
    src/business/BreakpointManager.cpp
    src/business/DisassemblyEngine.cpp
    src/business/SymbolResolver.cpp
    src/business/StackManager.cpp
    src/business/ThreadManager.cpp
    src/business/DumpManager.cpp
)

set(HANDLER_SOURCES
    src/handlers/DebugHandler.cpp
    src/handlers/RegisterHandler.cpp
    src/handlers/MemoryHandler.cpp
    src/handlers/BreakpointHandler.cpp
    src/handlers/DisassemblyHandler.cpp
    src/handlers/StackHandler.cpp
    src/handlers/ThreadHandler.cpp
    src/handlers/ModuleHandler.cpp
    src/handlers/EventCallbackHandler.cpp
    src/handlers/ScriptHandler.cpp
    src/handlers/ContextHandler.cpp
    src/handlers/DumpHandler.cpp
    src/handlers/ExpressionHandler.cpp
    src/handlers/AnalysisHandler.cpp
    src/handlers/AssemblerHandler.cpp
    src/handlers/BookmarkHandler.cpp
    src/handlers/PatchHandler.cpp
)

set(UTILS_SOURCES
    src/utils/StringUtils.cpp
    src/utils/ThreadPool.cpp
)

set(UI_SOURCES
    src/ui/ConfigEditor.cpp
    src/ui/ConfigEditor.rc
)

set(PLUGIN_SOURCES
    src/PluginEntry.cpp
)

# 主库
add_library(x64dbg_mcp SHARED
    ${CORE_SOURCES}
    ${COMMUNICATION_SOURCES}
    ${BUSINESS_SOURCES}
    ${HANDLER_SOURCES}
    ${UTILS_SOURCES}
    ${UI_SOURCES}
    ${PLUGIN_SOURCES}
)

# 包含目录
target_include_directories(x64dbg_mcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${XDBG_BUNDLED_SDK_DIR}
)

# 添加 SDK 定义
target_compile_definitions(x64dbg_mcp PRIVATE XDBG_SDK_AVAILABLE)
if(XDBG_ARCH STREQUAL "x64")
    target_compile_definitions(x64dbg_mcp PRIVATE XDBG_ARCH_X64)
    message(STATUS "Building for x64 (x64dbg)")
else()
    target_compile_definitions(x64dbg_mcp PRIVATE XDBG_ARCH_X86)
    message(STATUS "Building for x86 (x32dbg)")
endif()

# 链接库
if(XDBG_ARCH STREQUAL "x86")
    # x32dbg SDK 库名
    set(XDBG_BRIDGE_LIB "${XDBG_BUNDLED_SDK_DIR}/x32bridge.lib" CACHE STRING "Path to xdbg bridge lib (override if needed)")
    set(XDBG_MAIN_LIB "${XDBG_BUNDLED_SDK_DIR}/x32dbg.lib" CACHE STRING "Path to xdbg main lib (override if needed)")
else()
    # x64dbg SDK 库名
    set(XDBG_BRIDGE_LIB "${XDBG_BUNDLED_SDK_DIR}/x64bridge.lib" CACHE STRING "Path to xdbg bridge lib (override if needed)")
    set(XDBG_MAIN_LIB "${XDBG_BUNDLED_SDK_DIR}/x64dbg.lib" CACHE STRING "Path to xdbg main lib (override if needed)")
endif()

target_link_libraries(x64dbg_mcp PRIVATE
    nlohmann_json::nlohmann_json
    ws2_32
    ${XDBG_BRIDGE_LIB}
    ${XDBG_MAIN_LIB}
)

# 编译定义
if(ENABLE_LOGGING)
    target_compile_definitions(x64dbg_mcp PRIVATE ENABLE_LOGGING)
endif()

# Windows 特定设置
if(WIN32)
    target_compile_definitions(x64dbg_mcp PRIVATE
        _WIN32_WINNT=0x0601
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# 警告级别
if(MSVC)
    target_compile_options(x64dbg_mcp PRIVATE /W4 /utf-8)
else()
    target_compile_options(x64dbg_mcp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 导出符号
if(XDBG_ARCH STREQUAL "x64")
    set(PLUGIN_DIR_NAME "x64dbg-mcp")
    set(PLUGIN_DISPLAY_NAME_STR "x64dbg MCP Server")
    set(PLUGIN_BASENAME "x64dbg_mcp")
    set(PLUGIN_SUFFIX ".dp64")
else()
    set(PLUGIN_DIR_NAME "x32dbg-mcp")
    set(PLUGIN_DISPLAY_NAME_STR "x32dbg MCP Server")
    set(PLUGIN_BASENAME "x32dbg_mcp")
    set(PLUGIN_SUFFIX ".dp32")
endif()

target_compile_definitions(x64dbg_mcp PRIVATE 
    PLUGIN_DIR_NAME="${PLUGIN_DIR_NAME}"
    PLUGIN_DISPLAY_NAME="${PLUGIN_DISPLAY_NAME_STR}"
)

set_target_properties(x64dbg_mcp PROPERTIES
    OUTPUT_NAME "${PLUGIN_BASENAME}"
    SUFFIX "${PLUGIN_SUFFIX}"
    PREFIX ""
)

# 使用 DEF 文件导出符号
if(MSVC)
    target_sources(x64dbg_mcp PRIVATE src/plugin.def)
endif()

# 测试
if(BUILD_TESTS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests")
        enable_testing()
        add_subdirectory(tests)
    else()
        message(STATUS "Tests directory not found, skipping tests")
    endif()
endif()

# 安装
install(TARGETS x64dbg_mcp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    DESTINATION bin
)
