cmake_minimum_required(VERSION 3.16)

project(cpp_task_ctp LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CTP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CTP_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thrid_party/imgui")
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thrid_party/implot/implot-master")

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

# Dear ImGui is vendored in the repository.  The OpenGL3 backend includes its
# own small OpenGL loader, so this target does not require GLAD or GLEW.
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

target_include_directories(imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
)

# ImPlot uses Dear ImGui for real-time plots. Keep it as a separate target so
# non-chart UI code can continue to depend on imgui alone.
add_library(implot STATIC
    "${IMPLOT_DIR}/implot.cpp"
    "${IMPLOT_DIR}/implot_items.cpp"
)

target_include_directories(implot PUBLIC
    "${IMPLOT_DIR}"
)

target_link_libraries(implot PUBLIC
    imgui
)

add_library(ctp_md SHARED IMPORTED GLOBAL)
set_target_properties(ctp_md PROPERTIES
    IMPORTED_LOCATION "${CTP_LIB_DIR}/thostmduserapi_se.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CTP_INCLUDE_DIR}"
)

add_library(ctp_trader SHARED IMPORTED GLOBAL)
set_target_properties(ctp_trader PROPERTIES
    IMPORTED_LOCATION "${CTP_LIB_DIR}/thosttraderapi_se.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CTP_INCLUDE_DIR}"
)

add_executable(cpp_task_ctp
    src/main.cpp
    src/gui/GUI_app.cpp
    src/gui/BasePanel.cpp
    src/gui/PlotAxis.cpp
    src/gui/PanelManager.cpp
    src/gui/panels/mkt_panel.cpp
    src/gui/panels/chart_panel.cpp
    src/gui/panels/spread_panel.cpp
    src/mdspi.cpp
    src/tdspi.cpp
    src/DataBuffer.cpp
    src/indicator.cpp
    include/app_config.h
    include/DataBuffer.h
    include/gui/GUI_APP.h
    include/gui/GuiConfig.h
    include/gui/IPanel.h
    include/gui/BasePanel.h
    include/gui/PlotAxis.h
    include/gui/PanelManager.h
    include/gui/panels/MarketPanel.h
    include/gui/panels/ChartPanel.h
    include/gui/panels/SpreadPanel.h
    include/indicator.h
    include/mdspi.h
    include/tdspi.h
)

target_include_directories(cpp_task_ctp PRIVATE
    "${CTP_INCLUDE_DIR}"
)

target_compile_definitions(cpp_task_ctp PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(cpp_task_ctp PRIVATE
    ctp_md
    ctp_trader
    implot
    Threads::Threads
    ${CMAKE_DL_LIBS}
    rt
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(cpp_task_ctp PRIVATE "LINKER:--no-as-needed")
endif()

add_custom_command(TARGET cpp_task_ctp PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/flow/marketflow"
        "${CMAKE_CURRENT_SOURCE_DIR}/flow/tradeflow"
)

set_target_properties(cpp_task_ctp PROPERTIES
    BUILD_RPATH "${CTP_LIB_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
)
