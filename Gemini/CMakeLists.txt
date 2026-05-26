# ============================================================
# Plugin: Gemini Balance
# Displays Google Gemini API usage in the taskbar via
# the TrafficMonitor plugin interface.
#
# Auth: GCP Service Account → JWT → OAuth2 → Cloud Monitoring
# Queries: generativeai.googleapis.com/request_count
# ============================================================

add_library(GeminiBalance SHARED
    ${CMAKE_SOURCE_DIR}/PluginInterface.h
    PluginGeminiBalance.h
    PluginGeminiBalance.cpp
    BalanceItem.h
    BalanceItem.cpp
    dllmain.cpp
    resource.h
    PluginGeminiBalance.rc
)

target_include_directories(GeminiBalance PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(GeminiBalance PRIVATE winhttp bcrypt ncrypt crypt32)

target_link_options(GeminiBalance PRIVATE -static-libgcc -static-libstdc++)

set_target_properties(GeminiBalance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
