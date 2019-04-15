project(dap_httpd C)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)
set(CMAKE_C_STANDARD 11)
set(SUBMODULES_NO_BUILD ON)

add_subdirectory(libdap)
add_subdirectory(libdap-server-core)
add_subdirectory(libdap-server)

if(UNIX)
    add_definitions ("-DDAP_OS_LINUX")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ")
add_executable(${PROJECT_NAME} "main.c" "sig_unix_handler.c")

target_link_libraries(${PROJECT_NAME} dap_core dap_crypto dap_core_server dap_enc_server m pthread magic )
