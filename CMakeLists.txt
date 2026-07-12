cmake_minimum_required(VERSION 3.16)

project(cpp_task_ctp LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CTP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CTP_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")

find_package(Threads REQUIRED)

add_library(ctp_md SHARED IMPORTED GLOBAL)
set_target_properties(ctp_md PROPERTIES
    IMPORTED_LOCATION "${CTP_LIB_DIR}/thostmduserapi_se.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CTP_INCLUDE_DIR}"
)

add_library(ctp_trader SHARED IMPORTED GLOBAL)
set_target_properties(ctp_trader PROPERTIES
    IMPORTED_LOCATION "${CTP_LIB_DIR}/thosttraderapi_se.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CTP_INCLUDE_DIR}"
)

add_executable(cpp_task_ctp
    src/main.cpp
    src/mdspi.cpp
    src/tdspi.cpp
    src/DataBuffer.cpp
    src/indicator.cpp
    include/app_config.h
    include/DataBuffer.h
    include/indicator.h
    include/mdspi.h
    include/tdspi.h
)

target_include_directories(cpp_task_ctp PRIVATE
    "${CTP_INCLUDE_DIR}"
)

target_compile_definitions(cpp_task_ctp PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(cpp_task_ctp PRIVATE
    ctp_md
    ctp_trader
    Threads::Threads
    ${CMAKE_DL_LIBS}
    rt
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(cpp_task_ctp PRIVATE "LINKER:--no-as-needed")
endif()

add_custom_command(TARGET cpp_task_ctp PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/flow/marketflow"
        "${CMAKE_CURRENT_SOURCE_DIR}/flow/tradeflow"
)

set_target_properties(cpp_task_ctp PROPERTIES
    BUILD_RPATH "${CTP_LIB_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
)
