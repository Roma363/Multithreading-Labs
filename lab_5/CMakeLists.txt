# CMakeLists.txt for Lab 5

# Create executable for lab_5
add_executable(lab_5 main.cpp)

target_compile_features(lab_5 PRIVATE cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(lab_5 PRIVATE Threads::Threads)

# Optional: Set output directory for the executable
set_target_properties(lab_5 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
