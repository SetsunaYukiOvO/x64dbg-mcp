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
