include(FetchContent)

# ── ImGui (docking branch) ───────────────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        e05bd9deb9a73b67acb5c04b8a581e06a2d42657
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/freetype
)

target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
    GLEW::GLEW
    Freetype::Freetype
)

target_compile_definitions(imgui PUBLIC
    IMGUI_ENABLE_FREETYPE
    IMGUI_IMPL_OPENGL_LOADER_GLEW
)

# ── ImPlot ───────────────────────────────────────────────────────────
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.17
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(implot)

add_library(implot STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
)

target_include_directories(implot PUBLIC
    ${implot_SOURCE_DIR}
)

target_link_libraries(implot PUBLIC imgui)

# ── glm ──────────────────────────────────────────────────────────────
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    ON
)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# ── spdlog ──────────────────────────────────────────────────────────
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
    GIT_SHALLOW    ON
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# ── nlohmann/json ───────────────────────────────────────────────────
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    ON
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# ── glaze ──────────────────────────────────────────────────────────
FetchContent_Declare(glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG        v4.2.2
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(glaze)

# ── GeographicLib ───────────────────────────────────────────────────
FetchContent_Declare(geographiclib
    GIT_REPOSITORY https://github.com/geographiclib/geographiclib.git
    GIT_TAG        v2.3
    GIT_SHALLOW    ON
)
set(GEOGRAPHICLIB_LIB_TYPE STATIC CACHE STRING "" FORCE)
set(BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(BUILD_MANPAGES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(geographiclib)

# ── generator (procedural geometry) ─────────────────────────────────
FetchContent_Declare(generator
    GIT_REPOSITORY https://github.com/ilmola/generator.git
    GIT_TAG        ed17c6f7c86d03c215827def77d28a127f9e14ae
)
FetchContent_GetProperties(generator)
if(NOT generator_POPULATED)
    FetchContent_Populate(generator)
endif()

file(GLOB GENERATOR_SRC ${generator_SOURCE_DIR}/src/*.cpp)
add_library(generator STATIC ${GENERATOR_SRC})
target_include_directories(generator SYSTEM PUBLIC ${generator_SOURCE_DIR}/include)
target_compile_definitions(generator PUBLIC GENERATOR_USE_GLM GLM_ENABLE_EXPERIMENTAL)
target_compile_options(generator PRIVATE -w)
target_link_libraries(generator PUBLIC glm::glm)

# ── tinyobjloader (header-only OBJ parser) ─────────────────────────
FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    ON
)
FetchContent_GetProperties(tinyobjloader)
if(NOT tinyobjloader_POPULATED)
    FetchContent_Populate(tinyobjloader)
endif()
add_library(tinyobjloader INTERFACE)
target_include_directories(tinyobjloader SYSTEM INTERFACE ${tinyobjloader_SOURCE_DIR})
