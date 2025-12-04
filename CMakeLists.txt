cmake_minimum_required(VERSION 3.10)
project(jesenrpc VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add jesen dependency
add_subdirectory(vendor/jesen)

# Library sources
set(JESENRPC_SOURCES
    jesenrpc.c
)

# Create static library
add_library(jesenrpc STATIC ${JESENRPC_SOURCES})

target_include_directories(jesenrpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/jesen>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(jesenrpc PUBLIC jesen)

# Optional: Build tests
option(JESENRPC_BUILD_TESTS "Build tests" OFF)

if(JESENRPC_BUILD_TESTS)
    enable_testing()
    add_executable(test_jesenrpc tests/test_jesenrpc.c)
    target_link_libraries(test_jesenrpc PRIVATE jesenrpc)
    add_test(NAME test_jesenrpc COMMAND test_jesenrpc)
endif()

# Installation
install(TARGETS jesenrpc
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES jesenrpc.h DESTINATION include)
