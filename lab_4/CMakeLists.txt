# CMakeLists.txt for Lab 4

# Create executables for lab_4
add_executable(lab_4_server
    server.cpp
)

add_executable(lab_4_client
    client.cpp
)

foreach(target lab_4_server lab_4_client)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()

if (WIN32)
    target_link_libraries(lab_4_server ws2_32)
    target_link_libraries(lab_4_client ws2_32)
endif()
