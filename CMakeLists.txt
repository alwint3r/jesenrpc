cmake_minimum_required(VERSION 3.10)
project(jesenrpc VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Build options
option(JESENRPC_BUILD_SHARED "Build jesenrpc as a shared library" OFF)
option(JESENRPC_USE_EXTERNAL_JESEN
       "Use an externally provided jesen target instead of vendored copy" OFF)

set(JESENRPC_PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})

# Library sources
set(JESENRPC_SOURCES
    jesenrpc.c
)

set(JESENRPC_LIBRARY_TYPE STATIC)
if(JESENRPC_BUILD_SHARED)
    set(JESENRPC_LIBRARY_TYPE SHARED)
endif()

add_library(jesenrpc ${JESENRPC_LIBRARY_TYPE} ${JESENRPC_SOURCES})
add_library(jesenrpc::jesenrpc ALIAS jesenrpc)

if(JESENRPC_USE_EXTERNAL_JESEN)
    if(NOT TARGET jesen)
        find_package(jesen CONFIG QUIET)
    endif()
    if(NOT TARGET jesen)
        set(JESEN_LIBRARY "" CACHE FILEPATH "Path to a prebuilt jesen library")
        set(JESEN_INCLUDE_DIR "" CACHE PATH "Path to the jesen headers")
        if(JESEN_LIBRARY AND EXISTS "${JESEN_LIBRARY}" AND JESEN_INCLUDE_DIR)
            add_library(jesen UNKNOWN IMPORTED)
            set_target_properties(jesen PROPERTIES
                IMPORTED_LOCATION "${JESEN_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${JESEN_INCLUDE_DIR}"
            )
        endif()
    endif()
else()
    add_subdirectory(vendor/jesen)
    set_target_properties(jesen PROPERTIES POSITION_INDEPENDENT_CODE ON)
    list(APPEND JESENRPC_PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/jesen)
endif()

if(NOT TARGET jesen)
    message(FATAL_ERROR "jesen target not found. Provide an external jesen target, set JESEN_LIBRARY/JESEN_INCLUDE_DIR, or disable JESENRPC_USE_EXTERNAL_JESEN to use the vendored copy.")
endif()

target_include_directories(jesenrpc
    PUBLIC
        $<BUILD_INTERFACE:${JESENRPC_PUBLIC_INCLUDES}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(jesenrpc PUBLIC jesen)
if(JESENRPC_BUILD_SHARED)
    target_compile_definitions(jesenrpc
        PRIVATE JESENRPC_BUILDING_SHARED
        PUBLIC JESENRPC_SHARED
    )
endif()

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
    EXPORT jesenrpcTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(JESENRPC_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/jesenrpc")

install(EXPORT jesenrpcTargets
    FILE jesenrpcTargets.cmake
    NAMESPACE jesenrpc::
    DESTINATION ${JESENRPC_CONFIG_INSTALL_DIR}
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/jesenrpcConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/jesenrpcConfig.cmake
    INSTALL_DESTINATION ${JESENRPC_CONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/jesenrpcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/jesenrpcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/jesenrpcConfigVersion.cmake
    DESTINATION ${JESENRPC_CONFIG_INSTALL_DIR}
)

install(FILES jesenrpc.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
