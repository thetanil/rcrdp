cmake_minimum_required(VERSION 3.16)
project(rcrdp VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find FreeRDP 3.x
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREERDP REQUIRED freerdp3 freerdp-client3 winpr3)

# Add executable
add_executable(rcrdp
    src/main.c
    src/rdp_client.c
    src/commands.c
)

# Include directories
target_include_directories(rcrdp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${FREERDP_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(rcrdp
    ${FREERDP_LIBRARIES}
)

# Compiler flags
target_compile_options(rcrdp PRIVATE 
    ${FREERDP_CFLAGS_OTHER}
    -D_GNU_SOURCE
)

# Test executable
add_executable(test_connection
    tests/test_connection.c
    src/rdp_client.c
    src/commands.c
)

target_include_directories(test_connection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${FREERDP_INCLUDE_DIRS}
)

target_link_libraries(test_connection
    ${FREERDP_LIBRARIES}
)

target_compile_options(test_connection PRIVATE 
    ${FREERDP_CFLAGS_OTHER}
    -D_GNU_SOURCE
)

# Set test output to tests directory in build
set_target_properties(test_connection PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

# Install targets
install(TARGETS rcrdp
    RUNTIME DESTINATION bin
)