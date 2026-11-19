cmake_minimum_required(VERSION 3.16)
project(colibry-view VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(COLIBRY_BUILD_TESTS "Build tests" ON)
# Raven is found in this order: RAVEN_DIR (a source checkout), an installed package,
# and finally a download of a pinned Raven commit (needs network at configure time).
set(RAVEN_DIR "" CACHE PATH "Path to a Raven source checkout")
set(RAVEN_FETCH_COMMIT "89197438a59edde8efc38c9b48223e6721ec1816" CACHE STRING "Raven commit used when Raven is downloaded")

if(RAVEN_DIR)
    set(RAVEN_SOURCE_DIR "${RAVEN_DIR}")
else()
    find_package(raven 0.2 QUIET)
    if(NOT raven_FOUND)
        include(FetchContent)
        if(POLICY CMP0169)  # CMake 3.30+: Raven is added as an EXCLUDE_FROM_ALL subdirectory below
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Declare(raven
            GIT_REPOSITORY https://github.com/oscar-soirey/Raven.git
            GIT_TAG ${RAVEN_FETCH_COMMIT})
        FetchContent_GetProperties(raven)
        if(NOT raven_POPULATED)
            FetchContent_Populate(raven)
        endif()
        set(RAVEN_SOURCE_DIR "${raven_SOURCE_DIR}")
    endif()
endif()

if(DEFINED RAVEN_SOURCE_DIR)
    set(RAVEN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(RAVEN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(RAVEN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(RAVEN_BUILD_WEB OFF CACHE BOOL "" FORCE)
    add_subdirectory(${RAVEN_SOURCE_DIR} raven EXCLUDE_FROM_ALL)
endif()

# core/: the small shared base (start-up, XML loading, JSON, processes). Copied into every Colibry repo
# so that each one builds, packages and releases on its own.
add_library(colibry_core STATIC core/src/app.cpp core/src/process.cpp core/src/json.cpp)
target_include_directories(colibry_core PUBLIC core/include)
target_link_libraries(colibry_core PUBLIC raven)
target_include_directories(colibry_core PUBLIC ${RAVEN_SOURCE_DIR}/bindings/cpp)

file(GLOB APP_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/app/*.cpp)
list(REMOVE_ITEM APP_SRC ${CMAKE_SOURCE_DIR}/app/main.cpp)
if(APP_SRC)
    add_library(app_lib STATIC ${APP_SRC})
    target_include_directories(app_lib PUBLIC app)
    target_link_libraries(app_lib PUBLIC colibry_core)
else()
    add_library(app_lib INTERFACE)
    target_link_libraries(app_lib INTERFACE colibry_core)
endif()

add_executable(colibry-view app/main.cpp)
target_link_libraries(colibry-view PRIVATE app_lib)

install(TARGETS colibry-view RUNTIME DESTINATION bin)
file(GLOB XMLS ${CMAKE_SOURCE_DIR}/app/*.xml)
install(FILES ${XMLS} DESTINATION share/colibry/viewer)
install(FILES LICENSE DESTINATION share/licenses/colibry-view)

if(COLIBRY_BUILD_TESTS)
    enable_testing()
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE colibry_core)
    target_include_directories(test_core PRIVATE tests)
    add_test(NAME core COMMAND test_core)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_viewer.cpp)
        add_executable(test_viewer tests/test_viewer.cpp)
        target_link_libraries(test_viewer PRIVATE app_lib)
        target_include_directories(test_viewer PRIVATE tests)
        target_compile_definitions(test_viewer PRIVATE COLIBRY_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME viewer COMMAND test_viewer)
    endif()
endif()
