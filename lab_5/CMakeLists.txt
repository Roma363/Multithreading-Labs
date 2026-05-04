# CMakeLists.txt for Lab 5

add_executable(lab_5 main.cpp)

# Використовуємо PRIVATE для обох бібліотек (або об'єднуємо їх)
target_compile_features(lab_5 PRIVATE cxx_std_17)

find_package(Threads REQUIRED)

# Об'єднаний виклик лінкування бібліотек (найкраща практика)
target_link_libraries(lab_5 PRIVATE ws2_32 Threads::Threads)

# Встановлюємо директорію для вихідного файлу
set_target_properties(lab_5 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)