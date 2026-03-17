cmake_minimum_required(VERSION 3.24)
project(kiloscope LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── build configuration ─────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE        ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO  ON)

# ── dependencies ────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(Freetype REQUIRED)
include(cmake/FetchDependencies.cmake)

# ── target ──────────────────────────────────────────────────────────
file(GLOB_RECURSE KILOSCOPE_SRC CONFIGURE_DEPENDS src/*.cpp)
add_executable(kiloscope ${KILOSCOPE_SRC})

target_include_directories(kiloscope PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(kiloscope PRIVATE
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

target_compile_options(kiloscope PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-march=native>
)

target_link_libraries(kiloscope PRIVATE
    imgui implot glm::glm asio spdlog::spdlog nlohmann_json::nlohmann_json
    EnTT::EnTT generator magic_enum::magic_enum glaze::glaze tomlplusplus::tomlplusplus
    OpenGL::GL glfw GLEW::GLEW Freetype::Freetype
)
