cmake_minimum_required(VERSION 3.16)

project(GLTFConvert)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages installed via vcpkg
find_package(glm REQUIRED)
find_package(fastgltf REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Vulkan REQUIRED)

# Option: use static C runtime
option(USE_STATIC_CRT "Use static C runtime (MSVC: /MT, others: -static-libgcc -static-libstdc++)" ON)

find_path(MINIPACK_WRITER NAMES mini_pack_writer.h)

# Define imported library for MiniPack (adjust paths if your actual layout differs)
add_library(minipack_writer STATIC IMPORTED GLOBAL)
set_target_properties(minipack_writer PROPERTIES
    IMPORTED_LOCATION_DEBUG         "${MINIPACK_WRITER}/Debug/minipack_writer.lib"
    IMPORTED_LOCATION_RELEASE       "${MINIPACK_WRITER}/Release/minipack_writer.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${MINIPACK_WRITER}/RelWithDebInfo/minipack_writer.lib"
    IMPORTED_LOCATION_MINSIZEREL    "${MINIPACK_WRITER}/MinSizeRel/minipack_writer.lib"
    INTERFACE_INCLUDE_DIRECTORIES   "${MINIPACK_WRITER}"
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(MATH_FILES  math/AABB.h
                math/BoundingBox.h
                math/BoundingSphere.cpp
                math/TRS.h
                math/TRS.cpp
                math/OBB.cpp
                math/OBB.h)

SOURCE_GROUP("Math" FILES ${MATH_FILES})

set(COMMON_FILES    common/IndexType.h
                    common/VertexAttrib.h
                    common/VkFormat.cpp
                    common/VkFormat.h
                    common/PrimitiveType.h)

SOURCE_GROUP("Common" FILES ${COMMON_FILES})

# New gltf split headers
set(GLTF_FILES
    gltf/Geometry.h
    gltf/Primitive.h
    gltf/Material.h
    gltf/Mesh.h
    gltf/Node.h
    gltf/Scene.h
    gltf/Model.h
)
SOURCE_GROUP("gltf" FILES ${GLTF_FILES})

# New pure geometry split headers
set(PURE_GEOM_FILES
    pure/BoundsIndex.h
    pure/GeometryAttribute.h
    pure/GeometryIndicesMeta.h
    pure/Geometry.h
    pure/Material.h
    pure/MeshNode.h
    pure/SubMesh.h
    pure/Scene.h
    pure/ModelCore.h
    pure/ModelCore.cpp
)
SOURCE_GROUP("pure" FILES ${PURE_GEOM_FILES})

# New convert sources and header
set(CONVERT_FILES
    convert/UniqueGeometryMapping.h
    convert/CopyMaterials.cpp
    convert/CopyScenes.cpp
    convert/CopyNodes.cpp
    convert/GeometryMapping.cpp
    convert/CreateGeometry.cpp
    convert/BuildSubMeshes.cpp
    convert/AttachNodeSubMeshes.cpp
)
SOURCE_GROUP("convert" FILES ${CONVERT_FILES})

# Export split sources
set(EXPORT_FILES
    export/ExportGeometries.cpp
    export/ExportScene.cpp
    export/ExportPureModel.cpp
)
SOURCE_GROUP("export" FILES ${EXPORT_FILES})

# Add executable (include new modules for pure glTF data, importer and exporter)
add_executable(GLTFConvert
    main.cpp

    ${MATH_FILES}
    ${COMMON_FILES}
    ${GLTF_FILES}
    ${PURE_GEOM_FILES}
    ${CONVERT_FILES}
    ${EXPORT_FILES}

    Importer.cpp Importer.h
    Geometry.cpp
    GLTF2Pure.cpp
    MeshBounds.cpp MeshBounds.h
    Exporter.h
    MaterialExporter.cpp MaterialExporter.h
    # new split scene-local module and split headers
    SceneExporter.h SceneBinary.cpp SceneBuilder.cpp
    PureMaterial.h
)

# GLM SIMD/AVX2 宏定义（仅作用于目标）
target_compile_definitions(GLTFConvert PRIVATE
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_INTRINSICS
)

if (MINIPACK_WRITER)
    message(STATUS "Found mini_pack_writer.h: ${MINIPACK_WRITER}")
    target_include_directories(GLTFConvert PRIVATE "${MINIPACK_WRITER}")
else()
    message(FATAL_ERROR "mini_pack_writer.h not found. Please ensure the MiniPack library is available.")
endif()

# 编译器指令（目标级 + 生成期表达式）
# MSVC: 通用与按配置选项
if (MSVC)
    if(USE_STATIC_CRT)
        message(STATUS "Using static runtime on MSVC toolchain")
        # /MT (Release) 与 /MTd (Debug)
        set_property(TARGET GLTFConvert PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    else()
        # /MD (Release) 与 /MDd (Debug)
        set_property(TARGET GLTFConvert PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

# 统一设置目标编译选项（使用生成期表达式区分编译器与配置）
target_compile_options(GLTFConvert PRIVATE
    # MSVC 通用
    $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2 /fp:fast /openmp:llvm>
    # MSVC Debug/Release 特定
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi /Od /RTC1>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /Oi /Ot>
    # GCC/Clang
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-mavx2 -O3 -ffast-math -fopenmp>
)

# 非 MSVC：若请求静态运行时库，设置链接器标志
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(USE_STATIC_CRT)
        message(STATUS "Requesting static libgcc/libstdc++ on non-MSVC toolchains")
        set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    else()
        message(STATUS "Using dynamic runtime on non-MSVC toolchains")
    endif()
endif()

# Add Vulkan include directories so headers like <vulkan/vulkan.h> are found
if (Vulkan_FOUND)
    if (DEFINED Vulkan_INCLUDE_DIRS)
        include_directories(${Vulkan_INCLUDE_DIRS})
    elseif (TARGET Vulkan::Vulkan)
        get_target_property(_vulkan_includes Vulkan::Vulkan INTERFACE_INCLUDE_DIRECTORIES)
        if(_vulkan_includes)
            include_directories(${_vulkan_includes})
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
