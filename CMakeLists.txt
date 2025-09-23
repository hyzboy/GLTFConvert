cmake_minimum_required(VERSION 3.16)

project(GLTFConvert)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages installed via vcpkg
find_package(glm REQUIRED)
find_package(fastgltf REQUIRED)
find_package(nlohmann_json REQUIRED)

# Find Vulkan SDK (headers only; we won't link the library)
find_package(Vulkan REQUIRED)

# Add executable (include new modules for pure glTF data, importer and exporter)
add_executable(GLTFConvert
    main.cpp
    VKFormat.h
    PureGLTF.cpp PureGLTF.h
    Importer.cpp Importer.h
    Exporter.cpp Exporter.h
)

# Add Vulkan include directories so headers like <vulkan/vulkan.h> are found
if (Vulkan_FOUND)
    if (DEFINED Vulkan_INCLUDE_DIRS)
        target_include_directories(GLTFConvert PRIVATE ${Vulkan_INCLUDE_DIRS})
    elseif (TARGET Vulkan::Vulkan)
        get_target_property(_vulkan_includes Vulkan::Vulkan INTERFACE_INCLUDE_DIRECTORIES)
        if(_vulkan_includes)
            target_include_directories(GLTFConvert PRIVATE ${_vulkan_includes})
        endif()
    endif()
endif()

# Link libraries
target_link_libraries(GLTFConvert
    glm::glm
    fastgltf::fastgltf
    nlohmann_json::nlohmann_json
)
