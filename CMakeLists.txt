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

# Option: use static C runtime
option(USE_STATIC_CRT "Use static C runtime (MSVC: /MT, others: -static-libgcc -static-libstdc++)" ON)

find_path(MINIPACK_WRITER NAMES mini_pack_writer.h)

if (MINIPACK_WRITER)
    message(STATUS "Found mini_pack_writer.h: ${MINIPACK_WRITER}")
    include_directories(${MINIPACK_WRITER})

    link_directories(${MINIPACK_WRITER}/$<CONFIG>)

else()
    message(FATAL_ERROR "mini_pack_writer.h not found. Please ensure the MiniPack library is available.")
endif()

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
    SceneBinary.cpp SceneBinary.h
    # new split scene-local module and split headers
    SceneLocal.cpp SceneLocal.h
    MeshNodeTransform.h
    SubMesh.h
    MatrixEntry.h
    PureMaterial.h
)

# GLM SIMD/AVX2 优化相关宏定义
# 强制所有源文件启用 GLM 对齐和 SIMD
# 仅对 GLTFConvert 目标生效
# 宏定义
add_compile_definitions(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES GLM_FORCE_INTRINSICS)
# 编译器指令
if (MSVC)
    add_compile_options(/arch:AVX2 /fp:fast /openmp:llvm)

    if(USE_STATIC_CRT)
        message(STATUS "Using static runtime on MSVC toolchain")

        if($<CONFIG> STREQUAL "Debug")
            add_compile_options(/Zi /Od /RTC1 /MTd)
        else()
            add_compile_options(/O2 /Oi /Ot /MT)
        endif()
    endif(USE_STATIC_CRT)

elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Non-MSVC: if requested, instruct linker to use static libgcc/libstdc++ where possible
    if(USE_STATIC_CRT)
        message(STATUS "Requesting static libgcc/libstdc++ on non-MSVC toolchains")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    else()
        message(STATUS "Using dynamic runtime on non-MSVC toolchains")
    endif()

    add_compile_options(-mavx2 -O3 -ffast-math -fopenmp)
endif()

# Add Vulkan include directories so headers like <vulkan/vulkan.h> are found
if (Vulkan_FOUND)
    if (DEFINED Vulkan_INCLUDE_DIRS)
        include_directories(${Vulkan_INCLUDE_DIRS})
    elseif (TARGET Vulkan::Vulkan)
        get_target_property(_vulkan_includes Vulkan::Vulkan INTERFACE_INCLUDE_DIRECTORIES)
        if(_vulkan_includes)
            add_include_directories(${_vulkan_includes})
        endif()
    endif()
endif()

# Link libraries
target_link_libraries(GLTFConvert
    glm::glm
    fastgltf::fastgltf
    nlohmann_json::nlohmann_json
    minipack_writer
)
