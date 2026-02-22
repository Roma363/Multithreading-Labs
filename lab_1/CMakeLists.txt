# CMakeLists.txt for Lab 1

# Create executable for lab_1
add_executable(lab_1 main.cpp)

# Optional: Set output directory for the executable
set_target_properties(lab_1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
