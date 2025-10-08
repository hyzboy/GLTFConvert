cmake_minimum_required(VERSION 3.16)

project(GLTFConvert)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glm REQUIRED)
find_package(fastgltf REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Vulkan REQUIRED)

option(USE_STATIC_CRT "Use static C runtime (MSVC: /MT, others: -static-libgcc -static-libstdc++)" ON)

find_path(MINIPACK_WRITER NAMES mini_pack_writer.h)

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
                math/BoundingVolumes.h
                math/BoundingVolumes.cpp
                math/BoundingSphere.h
                math/BoundingSphere.cpp
                math/TRS.h
                math/TRS.cpp
                math/NodeTransform.h
                math/NodeTransform.cpp
                math/OBB.cpp
                math/OBB.h)
SOURCE_GROUP("Math" FILES ${MATH_FILES})

set(COMMON_FILES    common/IndexType.h
                    common/VertexAttrib.h
                    common/VkFormat.cpp
                    common/VkFormat.h
                    common/PrimitiveType.h
                    common/GeometryAttribute.h
                    common/FastGLTFConversions.h
                    common/FastGLTFConversions.cpp)
SOURCE_GROUP("Common" FILES ${COMMON_FILES})

set(GLTF_FILES
    gltf/GLTFGeometry.h
    gltf/GLTFPrimitive.h
    gltf/GLTFMaterial.h
    gltf/GLTFMesh.h
    gltf/GLTFNode.h
    gltf/GLTFScene.h
    gltf/GLTFModel.h
    gltf/GLTFImage.h
    gltf/GLTFTextureInfo.h
    gltf/GLTFSampler.h
    gltf/ToNodeTransform.h
    gltf/ToNodeTransform.cpp
)
SOURCE_GROUP("gltf" FILES ${GLTF_FILES})

set(GLTF_IMPORT_FILES
    gltf/import/GLTFImportMaterials.cpp
    gltf/import/GLTFImportPrimitives.cpp
    gltf/import/GLTFImportMeshes.cpp
    gltf/import/GLTFImportNodes.cpp
    gltf/import/GLTFImportScenes.cpp
    gltf/import/GLTFImportImages.cpp
    gltf/import/GLTFImportTextures.cpp
    gltf/import/GLTFImportSamplers.cpp
    gltf/import/GLTFOrientationNodes.cpp
    gltf/import/GLTFOrientationPrimitives.cpp
    gltf/import/GLTFImporter.cpp
    gltf/import/GLTFImporter.h
    gltf/import/GLTF2Pure.cpp
)
SOURCE_GROUP("gltf\\import" FILES ${GLTF_IMPORT_FILES})

set(PURE_GEOM_FILES
    pure/BoundsIndex.h
    pure/GeometryIndicesMeta.h
    pure/Geometry.h
    pure/Material.h
    pure/Mesh.h
    pure/Node.h
    pure/SubMesh.h
    pure/Scene.h
    pure/Model.h
    pure/Image.h
    pure/Texture.h
    pure/Sampler.h
)
SOURCE_GROUP("pure" FILES ${PURE_GEOM_FILES})

set(CONVERT_FILES
    convert/UniqueGeometryMapping.h
    convert/CopyMaterials.cpp
    convert/CopyScenes.cpp
    convert/CopyNodes.cpp
    convert/GeometryMapping.cpp
    convert/CreateGeometry.cpp
    convert/BuildPrimitives.cpp
    convert/AttachNodeSubMeshes.cpp
)
SOURCE_GROUP("convert" FILES ${CONVERT_FILES})

set(SCENE_EXPORT_INTERNAL_FILES
    export/SceneExportNames.h
    export/SceneExportNames.cpp
    export/SceneExportNodes.h
    export/SceneExportNodes.cpp
    export/SceneExportMaterials.h
    export/SceneExportMaterials.cpp
    export/SceneExportPrimitives.h
    export/SceneExportPrimitives.cpp
    export/SceneExportGeometries.h
    export/SceneExportGeometries.cpp
    export/SceneExportComputeBounds.h
    export/SceneExportComputeBounds.cpp
    export/SceneExportCollect.h
    export/SceneExportCollect.cpp
    export/SceneExportTransforms.h
    export/SceneExportTransforms.cpp
    export/SceneExportBounds.h
    export/SceneExportBounds.cpp
    export/SceneExportLists.h
    export/SceneExportLists.cpp
    export/SceneExportCollectTextures.h
    export/SceneExportCollectTextures.cpp
)
SOURCE_GROUP("export\\SceneBuildInternalsFine" FILES ${SCENE_EXPORT_INTERNAL_FILES})

set(EXPORT_FILES
    export/ExportGeometry.cpp
    export/ExportGeometries.cpp
    export/ExportPureModel.cpp
    export/ExportImages.cpp
    export/ExportImages.h
    export/SceneExportBuild.cpp
    export/SceneExportJson.cpp
    export/SceneExportPack.cpp
    export/MaterialExporter.cpp
    export/MaterialExporter.h
    export/MeshExporter.cpp
    export/MeshExporter.h
    export/ExportFileNames.h
    export/ExportFileNames.cpp
    export/SceneExportData.h
    ${SCENE_EXPORT_INTERNAL_FILES}
)
SOURCE_GROUP("export" FILES ${EXPORT_FILES})

add_executable(GLTFConvert
    main.cpp
    ${MATH_FILES}
    ${COMMON_FILES}
    ${GLTF_FILES}
    ${GLTF_IMPORT_FILES}
    ${PURE_GEOM_FILES}
    ${CONVERT_FILES}
    ${EXPORT_FILES}
)

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

if (MSVC)
    if(USE_STATIC_CRT)
        message(STATUS "Using static runtime on MSVC toolchain")
        set_property(TARGET GLTFConvert PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set_property(TARGET GLTFConvert PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

target_compile_options(GLTFConvert PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2 /fp:fast /openmp:llvm>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi /Od /RTC1>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /Oi /Ot>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-mavx2 -O3 -ffast-math -fopenmp>
)

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

target_link_libraries(GLTFConvert
    glm::glm
    fastgltf::fastgltf
    nlohmann_json::nlohmann_json
    minipack_writer
)
