# ============================================================
# Plugin: Codex Balance
# Displays OpenAI / Codex CLI usage and account balance in the
# taskbar via the TrafficMonitor plugin interface.
#
# Two data sources with priority:
#   Priority 1 — Session Cookie  (dashboard API, full billing)
#   Priority 2 — API Key         (official costs API, fallback)
# ============================================================

add_library(CodexBalance SHARED
    # Shared SDK (sits in project root)
    ${CMAKE_SOURCE_DIR}/PluginInterface.h
    # Plugin source
    PluginCodexBalance.h
    PluginCodexBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginCodexBalance.rc
)

target_include_directories(CodexBalance PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(CodexBalance PRIVATE winhttp)

target_link_options(CodexBalance PRIVATE -static-libgcc -static-libstdc++)

set_target_properties(CodexBalance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
