# CMakeLists.txt for Lab 4

# Create executables for lab_4
add_executable(lab_4_server
    cpp/src/server.cpp
)

add_executable(lab_4_client
    cpp/src/client.cpp
)

foreach(target lab_4_server lab_4_client)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include
    )
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()

target_link_libraries(lab_4_server ws2_32)
target_link_libraries(lab_4_client ws2_32)