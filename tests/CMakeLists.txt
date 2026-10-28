add_executable(architecture_register_names_x64 ArchitectureRegisterNamesTests.cpp)
target_include_directories(architecture_register_names_x64 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(architecture_register_names_x64 PRIVATE XDBG_ARCH_X64)
add_test(NAME architecture_register_names_x64 COMMAND architecture_register_names_x64)

add_executable(architecture_register_names_x86 ArchitectureRegisterNamesTests.cpp)
target_include_directories(architecture_register_names_x86 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(architecture_register_names_x86 PRIVATE XDBG_ARCH_X86)
add_test(NAME architecture_register_names_x86 COMMAND architecture_register_names_x86)

add_executable(breakpoint_architecture_x64 BreakpointArchitectureTests.cpp)
target_include_directories(breakpoint_architecture_x64 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(breakpoint_architecture_x64 PRIVATE XDBG_ARCH_X64)
target_compile_options(breakpoint_architecture_x64 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME breakpoint_architecture_x64 COMMAND breakpoint_architecture_x64)

add_executable(breakpoint_architecture_x86 BreakpointArchitectureTests.cpp)
target_include_directories(breakpoint_architecture_x86 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(breakpoint_architecture_x86 PRIVATE XDBG_ARCH_X86)
target_compile_options(breakpoint_architecture_x86 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME breakpoint_architecture_x86 COMMAND breakpoint_architecture_x86)

add_executable(stack_range StackRangeTests.cpp)
target_include_directories(stack_range PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(stack_range PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME stack_range COMMAND stack_range)

add_executable(target_value_validation_x64 TargetValueValidationTests.cpp)
target_include_directories(target_value_validation_x64 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(target_value_validation_x64 PRIVATE XDBG_ARCH_X64)
target_compile_options(target_value_validation_x64 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME target_value_validation_x64 COMMAND target_value_validation_x64)

add_executable(target_value_validation_x86 TargetValueValidationTests.cpp)
target_include_directories(target_value_validation_x86 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(target_value_validation_x86 PRIVATE XDBG_ARCH_X86)
target_compile_options(target_value_validation_x86 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME target_value_validation_x86 COMMAND target_value_validation_x86)

add_executable(request_address_validation_x64
    RequestAddressValidationTests.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RequestValidator.cpp
)
target_include_directories(request_address_validation_x64 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(request_address_validation_x64 PRIVATE XDBG_ARCH_X64)
target_compile_options(request_address_validation_x64 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(request_address_validation_x64 PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME request_address_validation_x64 COMMAND request_address_validation_x64)

add_executable(request_address_validation_x86
    RequestAddressValidationTests.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RequestValidator.cpp
)
target_include_directories(request_address_validation_x86 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(request_address_validation_x86 PRIVATE XDBG_ARCH_X86)
target_compile_options(request_address_validation_x86 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(request_address_validation_x86 PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME request_address_validation_x86 COMMAND request_address_validation_x86)

add_executable(register_value_validation_x64 RegisterValueValidationTests.cpp)
target_include_directories(register_value_validation_x64 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(register_value_validation_x64 PRIVATE XDBG_ARCH_X64)
target_compile_options(register_value_validation_x64 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME register_value_validation_x64 COMMAND register_value_validation_x64)

add_executable(register_value_validation_x86 RegisterValueValidationTests.cpp)
target_include_directories(register_value_validation_x86 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(register_value_validation_x86 PRIVATE XDBG_ARCH_X86)
target_compile_options(register_value_validation_x86 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME register_value_validation_x86 COMMAND register_value_validation_x86)

add_executable(breakpoint_address_entry_x86
    BreakpointAddressEntryTests.cpp
    ${CMAKE_SOURCE_DIR}/src/handlers/BreakpointHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RequestValidator.cpp
)
target_include_directories(breakpoint_address_entry_x86 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(breakpoint_address_entry_x86 PRIVATE XDBG_ARCH_X86 NOMINMAX)
target_compile_options(breakpoint_address_entry_x86 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(breakpoint_address_entry_x86 PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME breakpoint_address_entry_x86 COMMAND breakpoint_address_entry_x86)

add_executable(pe_validation PEValidationTests.cpp)
target_include_directories(pe_validation PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(pe_validation PRIVATE NOMINMAX)
add_test(NAME pe_validation COMMAND pe_validation)

add_executable(http_headers HttpHeadersTests.cpp)
target_include_directories(http_headers PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME http_headers COMMAND http_headers)

add_executable(debug_execution DebugExecutionTests.cpp ${CMAKE_SOURCE_DIR}/src/business/DebugController.cpp)
target_include_directories(debug_execution PRIVATE ${CMAKE_SOURCE_DIR}/src ${XDBG_BUNDLED_SDK_DIR})
target_compile_definitions(debug_execution PRIVATE XDBG_SDK_AVAILABLE NOMINMAX WIN32_LEAN_AND_MEAN BRIDGE_IMPEXP= PLUG_IMPEXP=)
if(XDBG_ARCH STREQUAL "x64")
    target_compile_definitions(debug_execution PRIVATE XDBG_ARCH_X64)
else()
    target_compile_definitions(debug_execution PRIVATE XDBG_ARCH_X86)
endif()
target_compile_options(debug_execution PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME debug_execution COMMAND debug_execution)

add_executable(uint32_validation UInt32ValidationTests.cpp)
target_include_directories(uint32_validation PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(uint32_validation PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME uint32_validation COMMAND uint32_validation)

add_executable(http_transport HttpTransportTests.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/MCPHttpServer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MCPToolRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MCPResourceRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MCPPromptRegistry.cpp)
target_include_directories(http_transport PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(http_transport PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(http_transport PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(http_transport PRIVATE nlohmann_json::nlohmann_json ws2_32 bcrypt)
add_test(NAME http_transport COMMAND http_transport)
set_tests_properties(http_transport PROPERTIES TIMEOUT 30)

add_executable(breakpoint_snapshot BreakpointSnapshotTests.cpp ${CMAKE_SOURCE_DIR}/src/business/BreakpointManager.cpp)
target_include_directories(breakpoint_snapshot PRIVATE ${CMAKE_SOURCE_DIR}/src ${XDBG_BUNDLED_SDK_DIR})
target_compile_definitions(breakpoint_snapshot PRIVATE XDBG_SDK_AVAILABLE NOMINMAX WIN32_LEAN_AND_MEAN BRIDGE_IMPEXP= PLUG_IMPEXP=)
if(XDBG_ARCH STREQUAL "x64")
    target_compile_definitions(breakpoint_snapshot PRIVATE XDBG_ARCH_X64)
else()
    target_compile_definitions(breakpoint_snapshot PRIVATE XDBG_ARCH_X86)
endif()
target_compile_options(breakpoint_snapshot PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME breakpoint_snapshot COMMAND breakpoint_snapshot)

add_executable(pe_rebuild PERebuildTests.cpp ${CMAKE_SOURCE_DIR}/src/business/PEDump.cpp)
target_include_directories(pe_rebuild PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(pe_rebuild PRIVATE NOMINMAX)
if(XDBG_ARCH STREQUAL "x64")
    target_compile_definitions(pe_rebuild PRIVATE XDBG_ARCH_X64)
else()
    target_compile_definitions(pe_rebuild PRIVATE XDBG_ARCH_X86)
endif()
target_compile_options(pe_rebuild PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME pe_rebuild COMMAND pe_rebuild)

add_executable(memory_search MemorySearchTests.cpp ${CMAKE_SOURCE_DIR}/src/business/MemoryManager.cpp)
target_include_directories(memory_search PRIVATE ${CMAKE_SOURCE_DIR}/src ${XDBG_BUNDLED_SDK_DIR})
target_compile_definitions(memory_search PRIVATE XDBG_SDK_AVAILABLE NOMINMAX WIN32_LEAN_AND_MEAN BRIDGE_IMPEXP= PLUG_IMPEXP=)
if(XDBG_ARCH STREQUAL "x64")
    target_compile_definitions(memory_search PRIVATE XDBG_ARCH_X64)
else()
    target_compile_definitions(memory_search PRIVATE XDBG_ARCH_X86)
endif()
target_compile_options(memory_search PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME memory_search COMMAND memory_search)

add_executable(hex_bytes HexBytesTests.cpp)
target_include_directories(hex_bytes PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(hex_bytes PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME hex_bytes COMMAND hex_bytes)

foreach(arch x64 x86)
    string(TOUPPER ${arch} arch_upper)
    add_executable(general_registers_${arch} GeneralRegistersTests.cpp ${CMAKE_SOURCE_DIR}/src/business/RegisterManager.cpp)
    target_include_directories(general_registers_${arch} PRIVATE ${CMAKE_SOURCE_DIR}/src ${XDBG_BUNDLED_SDK_DIR})
    target_compile_definitions(general_registers_${arch} PRIVATE XDBG_ARCH_${arch_upper} XDBG_SDK_AVAILABLE NOMINMAX WIN32_LEAN_AND_MEAN BRIDGE_IMPEXP= PLUG_IMPEXP=)
    target_compile_options(general_registers_${arch} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    add_test(NAME general_registers_${arch} COMMAND general_registers_${arch})
endforeach()
