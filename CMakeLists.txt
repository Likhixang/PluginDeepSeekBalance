cmake_minimum_required(VERSION 3.10)
project(PluginDeepSeekBalance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This plugin is only supported on Windows")
endif()

add_library(${PROJECT_NAME} SHARED
    PluginInterface.h
    PluginDeepSeekBalance.h
    PluginDeepSeekBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginDeepSeekBalance.rc
)

target_link_libraries(${PROJECT_NAME} PRIVATE winhttp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
