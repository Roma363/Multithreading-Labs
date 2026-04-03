# CMakeLists.txt for Lab 3

# Create executable for lab_3
add_executable(lab_3
    main.cpp
    thread_pool.cpp
    queue.cpp
    stats_tracker.cpp
    worker.cpp
)

set_target_properties(lab_3 PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Optional: Set output directory for the executable
set_target_properties(lab_3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
