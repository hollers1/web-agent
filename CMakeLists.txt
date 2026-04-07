cmake_minimum_required(VERSION 3.10)
project(WebAgent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable deprecation warnings as errors for CPR
add_compile_options(-Wno-error=deprecated-declarations)

# Fetch CPR
include(FetchContent)
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.11.0
)
FetchContent_MakeAvailable(cpr)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# Add source files
set(SOURCES
    src/main.cpp
    src/Config.cpp
    src/Logger.cpp
    src/ProcessExecutor.cpp
    src/Agent.cpp
)

add_executable(web_agent ${SOURCES})

target_link_libraries(web_agent
    cpr::cpr
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    ${CMAKE_THREAD_LIBS_INIT}
)