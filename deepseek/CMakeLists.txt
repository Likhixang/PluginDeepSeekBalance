# ============================================================
# Plugin: DeepSeek Balance
# Displays DeepSeek API account balance in the taskbar via
# the TrafficMonitor plugin interface.
# ============================================================

add_library(DeepSeekBalance SHARED
    # Shared SDK (sits in project root, included via parent path)
    ${CMAKE_SOURCE_DIR}/PluginInterface.h
    # Plugin source
    PluginDeepSeekBalance.h
    PluginDeepSeekBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginDeepSeekBalance.rc
)

target_include_directories(DeepSeekBalance PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(DeepSeekBalance PRIVATE winhttp)

# Static link MinGW runtime so the DLL has no external DLL dependencies
target_link_options(DeepSeekBalance PRIVATE -static-libgcc -static-libstdc++)

set_target_properties(DeepSeekBalance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
