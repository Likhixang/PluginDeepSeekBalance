# ============================================================
# Plugin: MiniMax Balance
# Displays MiniMax Token Plan usage in the taskbar via
# the TrafficMonitor plugin interface.
#
# Auth: Bearer API Token
# API:  GET /v1/token_plan/remains
#       (Intl: api.minimax.io / China: api.minimaxi.com)
# ============================================================

add_library(MiniMaxBalance SHARED
    ${CMAKE_SOURCE_DIR}/PluginInterface.h
    PluginMiniMaxBalance.h
    PluginMiniMaxBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginMiniMaxBalance.rc
)

target_include_directories(MiniMaxBalance PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(MiniMaxBalance PRIVATE winhttp)

target_link_options(MiniMaxBalance PRIVATE -static-libgcc -static-libstdc++)

set_target_properties(MiniMaxBalance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
