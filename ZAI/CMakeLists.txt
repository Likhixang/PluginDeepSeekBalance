# ============================================================
# Plugin: Z.AI Balance
# Displays Z.AI (Zhipu AI / GLM Coding Plan) token usage
# in the taskbar via the TrafficMonitor plugin interface.
#
# Auth: Bearer API Token
# API:  GET /api/monitor/usage/quota/limit
#       (Intl: api.z.ai / China: open.bigmodel.cn)
# ============================================================

add_library(ZAIBalance SHARED
    ${CMAKE_SOURCE_DIR}/PluginInterface.h
    PluginZAIBalance.h
    PluginZAIBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginZAIBalance.rc
)

target_include_directories(ZAIBalance PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(ZAIBalance PRIVATE winhttp)

target_link_options(ZAIBalance PRIVATE -static-libgcc -static-libstdc++)

set_target_properties(ZAIBalance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
