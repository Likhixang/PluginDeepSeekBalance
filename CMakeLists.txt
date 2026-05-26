cmake_minimum_required(VERSION 3.10)
project(AILiv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This project is only supported on Windows")
endif()

# ============================================================
# AI Liv - TrafficMonitor AI plugin collection
# Each AI provider gets its own subdirectory with a DLL target.
# Shared SDK header (PluginInterface.h) and future utility code
# live in the project root.
# ============================================================

add_subdirectory(DeepSeek)
add_subdirectory(Codex)
add_subdirectory(Claude)
add_subdirectory(Gemini)
