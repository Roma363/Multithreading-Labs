cmake_minimum_required(VERSION 3.10)
project(lab_5 CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prevent in-source builds
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed. Create a separate build directory.")
endif()

# Ensure this project is built only on Windows
if (NOT WIN32)
  message(FATAL_ERROR "lab_5 is Windows-only (WinSock + Win32 threads).")
endif()

# Source files
set(SOURCES
    src/server.cpp
    src/cache.cpp
)

# Create executable for the server
add_executable(lab_5_server ${SOURCES})

# Match the Makefile flags: -Wall -Wextra -O2 (or MSVC equivalents)
if (MSVC)
  target_compile_options(lab_5_server PRIVATE /W4 /O2)
else()
  target_compile_options(lab_5_server PRIVATE -Wall -Wextra -O2)
endif()

# Link with WinSock library
target_link_libraries(lab_5_server PRIVATE ws2_32)

# Ensure the include directory is searched for headers
# (and current dir for legacy reasons)
target_include_directories(lab_5_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})

# Place the built binary inside the global CMake build directory
set_target_properties(lab_5_server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# --- Копіювання public/ у спільну build-директорію ---
add_custom_command(TARGET lab_5_server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/public"
    "${CMAKE_BINARY_DIR}/public"
  COMMENT "Copying static files to build directory"
)
