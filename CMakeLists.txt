cmake_minimum_required(VERSION 3.21)

project(colibry-viewer
    VERSION 0.2.0
    DESCRIPTION "Viewer for the Colibry"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(Colibry_BUILD_TESTS "Build unit tests" ON)
option(Colibry_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(_qt_components Widgets)
if(Colibry_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ColibryApps)

if(Colibry_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common)
add_library(gus_fv_core STATIC
    src/core/TextLoader.h
    src/core/TextLoader.cpp
    src/core/HexDump.h
    src/core/HexDump.cpp
    src/core/FileKind.h
    src/core/FileKind.cpp
    src/core/FolderFiles.h
    src/core/FolderFiles.cpp
)
target_include_directories(gus_fv_core PUBLIC src)
target_link_libraries(gus_fv_core PUBLIC Qt6::Widgets)
colibry_target_defaults(gus_fv_core)

add_library(gus_fv_ui STATIC
    src/ViewerWindow.h
    src/ViewerWindow.cpp
)
target_include_directories(gus_fv_ui PUBLIC src)
target_link_libraries(gus_fv_ui PUBLIC gus_fv_core colibry_common Qt6::Widgets)
colibry_target_defaults(gus_fv_ui)

add_executable(colibry-viewer src/main.cpp)
target_link_libraries(colibry-viewer PRIVATE gus_fv_ui)
colibry_target_defaults(colibry-viewer)

colibry_install_app(colibry-viewer
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-viewer.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-viewer.svg)

if(Colibry_BUILD_TESTS)
    colibry_add_test(fv_core SOURCES tests/test_core.cpp LINK gus_fv_core)
    colibry_add_test(fv_ui SOURCES tests/test_ui.cpp LINK gus_fv_ui)
endif()
