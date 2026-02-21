cmake_minimum_required(VERSION 3.10)
project(Labs)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories for each lab
# Each lab will be compiled independently
add_subdirectory(lab_1)
# add_subdirectory(lab_2)
# add_subdirectory(lab_3)
# add_subdirectory(lab_4)