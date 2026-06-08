cmake_minimum_required(VERSION 4.0)

project(tewi VERSION 0.1 LANGUAGES CXX)

# Options definition to enable logging and trace features
option(TEWI_ENABLE_LOGGING "Enable logging in Tewi" ON)
option(TEWI_ENABLE_TRACE "Enable trace logging in Tewi" OFF)

# SQlite3
find_package(SQLite3 REQUIRED)

if(NOT TARGET SQLite3::SQLite3) # CMake < 4.3
  add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

add_library(${PROJECT_NAME})
# Alias target for consistent internal and external usage
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# This CMakeLists is used to define the source files
add_subdirectory(src)

if(TEWI_ENABLE_LOGGING)
    target_compile_definitions(${PROJECT_NAME} PUBLIC TEWI_LOG)
endif()

if(TEWI_ENABLE_TRACE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC TEWI_TRACE)
endif()

# Apply LTO only if supported by the host
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message("${PROJECT_NAME}: Interprocedural Optimization (LTO) is enabled")
else()
    message("${PROJECT_NAME}: Interprocedural Optimization (LTO) is disabled")
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC SQLite3::SQLite3)

target_include_directories(${PROJECT_NAME}
        INTERFACE "${PROJECT_SOURCE_DIR}/include"
        PRIVATE "${PROJECT_SOURCE_DIR}/src")

# Build tests only if this is the top-level project to avoid issues when included as a submodule
if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()