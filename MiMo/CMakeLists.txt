# ============================================================
# Plugin: MiMo Balance
# Displays Xiaomi MiMo Token Plan info in the taskbar via
# the TrafficMonitor plugin interface.
#
# Auth: API Key (sk-xxxxx or tp-xxxxx)
# Note: MiMo has NO public balance/usage API.
# This plugin shows manual plan info (tier + credits).
# Best-effort undocumented endpoint queries are attempted.
# ============================================================

add_library(MiMoBalance SHARED
    ${CMAKE_SOURCE_DIR}/PluginInterface.h
    PluginMiMoBalance.h
    PluginMiMoBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginMiMoBalance.rc
)

target_include_directories(MiMoBalance PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(MiMoBalance PRIVATE winhttp)

target_link_options(MiMoBalance PRIVATE -static-libgcc -static-libstdc++)

set_target_properties(MiMoBalance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
