cmake_minimum_required(VERSION 3.16)

project(GLTFConvert)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages installed via vcpkg
find_package(glm REQUIRED)
find_package(fastgltf REQUIRED)
find_package(nlohmann_json REQUIRED)
#find_path(V_HACD_INCLUDE_DIRS "VHACD.h")

# Find Vulkan SDK (headers only; we won't link the library)
find_package(Vulkan REQUIRED)

#include_directories(${V_HACD_INCLUDE_DIRS})

# Add executable (include new modules for pure glTF data, importer and exporter)
add_executable(GLTFConvert
    main.cpp
    VKFormat.cpp VKFormat.h
    VertexAttrib.h
    PrimitiveType.h
    AABB.h
    OBB.cpp OBB.h
    BoundingBox.h BoundingSphere.cpp
    PureGLTF.cpp PureGLTF.h
    Importer.cpp Importer.h
    Geometry.cpp Geometry.h
    StaticMesh.cpp StaticMesh.h
    MeshBounds.cpp MeshBounds.h
    Exporter.cpp Exporter.h
)

# GLM SIMD/AVX2 优化相关宏定义
# 强制所有源文件启用 GLM 对齐和 SIMD
# 仅对 GLTFConvert 目标生效
# 宏定义
 target_compile_definitions(GLTFConvert PRIVATE GLM_FORCE_DEFAULT_ALIGNED_GENTYPES GLM_FORCE_INTRINSICS)
# 编译器指令
if (MSVC)
    target_compile_options(GLTFConvert PRIVATE /arch:AVX2 /fp:fast /openmp:llvm)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(GLTFConvert PRIVATE -mavx2 -O3 -ffast-math -fopenmp)
endif()

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
