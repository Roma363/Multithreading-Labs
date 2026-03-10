# CMakeLists.txt for Lab 2

# Create executable for lab_2
add_executable(lab_2 main.cpp)

# Optional: Set output directory for the executable
set_target_properties(lab_2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
