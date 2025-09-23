cmake_minimum_required(VERSION 3.16)

project(GLTFConvert)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages installed via vcpkg
find_package(glm REQUIRED)
find_package(fastgltf REQUIRED)
find_package(nlohmann_json REQUIRED)

# Add executable
add_executable(GLTFConvert main.cpp GLTFExporter.cpp GLTFExporter.h)

# Link libraries
target_link_libraries(GLTFConvert
    glm::glm
    fastgltf::fastgltf
    nlohmann_json::nlohmann_json
)
