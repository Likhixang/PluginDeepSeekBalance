# ============================================================
# Plugin: Claude Balance
# Displays Anthropic Claude API usage and cost in the taskbar
# via the TrafficMonitor plugin interface.
#
# Auth: Admin API Key (sk-ant-admin-...) — official, documented.
#   Cost:  GET /v1/organizations/cost_report
#   Usage: GET /v1/organizations/usage_report/claude_code
# ============================================================

add_library(ClaudeBalance SHARED
    ${CMAKE_SOURCE_DIR}/PluginInterface.h
    PluginClaudeBalance.h
    PluginClaudeBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginClaudeBalance.rc
)

target_include_directories(ClaudeBalance PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(ClaudeBalance PRIVATE winhttp)

target_link_options(ClaudeBalance PRIVATE -static-libgcc -static-libstdc++)

set_target_properties(ClaudeBalance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
