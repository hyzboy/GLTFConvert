cmake_minimum_required(VERSION 3.16)

project(GLTFConvert)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glm REQUIRED)
find_package(fastgltf QUIET)
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
    gltf/GLTFTexture.h
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
    gltf/import/SamplerMapping.cpp
    gltf/import/SamplerMapping.h
)
SOURCE_GROUP("gltf\\import" FILES ${GLTF_IMPORT_FILES})

set(FBX_FILES
    fbx/FBXModel.h
)
SOURCE_GROUP("fbx" FILES ${FBX_FILES})

set(FBX_IMPORT_FILES
    fbx/import/FBXImporter.h
    fbx/import/FBXImporter.cpp
)
SOURCE_GROUP("fbx\\import" FILES ${FBX_IMPORT_FILES})

# FBX geometry processing moved to its own source
list(APPEND FBX_IMPORT_FILES
    fbx/import/FBXGeometry.h
    fbx/import/FBXGeometry.cpp
)
SOURCE_GROUP("fbx\\import" FILES fbx/import/FBXGeometry.h fbx/import/FBXGeometry.cpp)

list(APPEND FBX_IMPORT_FILES
    fbx/import/FBX2Pure.h
    fbx/import/FBX2Pure.cpp
)
SOURCE_GROUP("fbx\\import" FILES fbx/import/FBX2Pure.h fbx/import/FBX2Pure.cpp)

list(APPEND FBX_IMPORT_FILES
    fbx/import/FBXExpand_ByControlPoint.cpp
    fbx/import/FBXExpand_ByPolygonVertex.cpp
    fbx/import/FBXExpand_ByPolygon.cpp
)
SOURCE_GROUP("fbx\\import" FILES fbx/import/FBXExpand_ByControlPoint.cpp fbx/import/FBXExpand_ByPolygonVertex.cpp fbx/import/FBXExpand_ByPolygon.cpp)

# FBX material converter
list(APPEND FBX_IMPORT_FILES
    fbx/import/FBXMaterialConverter.cpp
)
SOURCE_GROUP("fbx\\import" FILES fbx/import/FBXMaterialConverter.cpp fbx/import/FBXMaterialConverter.h)

list(APPEND FBX_IMPORT_FILES
    fbx/import/FBXMaterialMap.cpp
)
SOURCE_GROUP("fbx\\import" FILES fbx/import/FBXMaterialMap.cpp fbx/import/FBXMaterialMap.h)

set(CONVERT_FILES
    gltf/convert/UniqueGeometryMapping.h
    gltf/convert/CopyMaterials.cpp
    gltf/convert/CopyScenes.cpp
    gltf/convert/CopyNodes.cpp
    gltf/convert/GeometryMapping.cpp
    gltf/convert/CreateGeometry.cpp
    gltf/convert/BuildPrimitives.cpp
    gltf/convert/BuildMeshes.cpp
    gltf/convert/ComputeMeshBounds.cpp
)
SOURCE_GROUP("gltf\\convert" FILES ${CONVERT_FILES})

set(PURE_GEOM_FILES
    pure/BoundsIndex.h
    pure/GeometryIndicesMeta.h
    pure/Geometry.h
    pure/Material.h
    pure/PBRMaterial.h
    pure/PBRMaterial.cpp
    pure/Mesh.h
    pure/Node.h
    pure/Primitive.h
    pure/Scene.h
    pure/Model.h
    pure/Image.h
    pure/Texture.h
    pure/Sampler.h
)
SOURCE_GROUP("pure" FILES ${PURE_GEOM_FILES})

# Additional pure material/FBX material headers
list(APPEND PURE_GEOM_FILES
    pure/FBXMaterial.h
    pure/LambertMaterial.h
    pure/PhongMaterial.h
    pure/SpecGlossMaterial.h
)
SOURCE_GROUP("pure" FILES pure/FBXMaterial.h pure/LambertMaterial.h pure/PhongMaterial.h pure/SpecGlossMaterial.h)

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
    export/BoundingVolumesJson.h
    export/BoundingVolumesJson.cpp
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
    export/ImageMime.cpp
    export/ImageMime.h
    export/ImageUsage.cpp
    export/ImageUsage.h
    export/SanitizeName.cpp
    export/SanitizeName.h
    export/SamplerStrings.cpp
    export/SamplerStrings.h
    export/SceneExportData.h
    ${SCENE_EXPORT_INTERNAL_FILES}
)
SOURCE_GROUP("export" FILES ${EXPORT_FILES})

set(PURE_MODEL_FILES
    ${MATH_FILES}
    ${COMMON_FILES}
    ${PURE_GEOM_FILES}
    ${EXPORT_FILES}
)

add_library(PureModel STATIC ${PURE_MODEL_FILES})

target_compile_definitions(PureModel PRIVATE
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_INTRINSICS
)

target_link_libraries(PureModel
    glm::glm
    nlohmann_json::nlohmann_json
)

add_executable(GLTFConvert
    gltf/main_gltf.cpp
    TexConv.cpp
    ${GLTF_FILES}
    ${GLTF_IMPORT_FILES}
    ${CONVERT_FILES}
)

target_compile_definitions(GLTFConvert PRIVATE
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_INTRINSICS
)
# Prevent FBX SDK headers from injecting 'using namespace' globally when included in these targets
# (FBX SDK namespace macros not forced here)

if (MINIPACK_WRITER)
    message(STATUS "Found mini_pack_writer.h: ${MINIPACK_WRITER}")
    target_include_directories(PureModel PRIVATE "${MINIPACK_WRITER}")
else()
    message(FATAL_ERROR "mini_pack_writer.h not found. Please ensure the MiniPack library is available.")
endif()

# Optional: FBX SDK support (Autodesk)
# Set FBX_SDK_ROOT to the SDK installation root (it should contain "include" and "lib" subfolders).
set(FBX_SDK_ROOT "" CACHE PATH "Path to Autodesk FBX SDK root (contains include/ and lib/)")

find_path(FBX_SDK_INCLUDE_DIR
    NAMES fbxsdk.h
    HINTS ${FBX_SDK_ROOT}
    PATH_SUFFIXES include include/fbxsdk
)

# Track whether FBX was located and what to link
set(FBX_FOUND OFF)
set(FBX_LIB_TARGET "")

# Prefer the plain libfbxsdk.lib (avoid md/mt variants)
set(FBX_LIB_DEBUG_PATH "")
set(FBX_LIB_RELEASE_PATH "")
if (FBX_SDK_ROOT)
    file(GLOB FBX_POSSIBLE_LIBS
        "${FBX_SDK_ROOT}/lib/**/libfbxsdk.lib"
        "${FBX_SDK_ROOT}/lib/**/fbxsdk.lib"
    )
    list(REMOVE_DUPLICATES FBX_POSSIBLE_LIBS)

    foreach(_lib ${FBX_POSSIBLE_LIBS})
        string(TOLOWER "${_lib}" _lib_low)
        if(_lib_low MATCHES "[\\/](debug|dbg)[\\/]")
            if(NOT FBX_LIB_DEBUG_PATH)
                set(FBX_LIB_DEBUG_PATH "${_lib}")
            endif()
        elseif(_lib_low MATCHES "[\\/](release)[\\/]")
            if(NOT FBX_LIB_RELEASE_PATH)
                set(FBX_LIB_RELEASE_PATH "${_lib}")
            endif()
        else()
            # prefer non-debug as release lib if release not set
            if(NOT FBX_LIB_RELEASE_PATH)
                set(FBX_LIB_RELEASE_PATH "${_lib}")
            endif()
        endif()
    endforeach()

    # If only one lib found, use it for both configs
    if(FBX_POSSIBLE_LIBS)
        if(NOT FBX_LIB_DEBUG_PATH)
            list(GET FBX_POSSIBLE_LIBS 0 FBX_LIB_DEBUG_PATH)
        endif()
        if(NOT FBX_LIB_RELEASE_PATH)
            list(GET FBX_POSSIBLE_LIBS 0 FBX_LIB_RELEASE_PATH)
        endif()
    endif()
endif()

if (FBX_SDK_INCLUDE_DIR AND (FBX_LIB_DEBUG_PATH OR FBX_LIB_RELEASE_PATH))
    message(STATUS "Found FBX SDK include: ${FBX_SDK_INCLUDE_DIR}")
    if(FBX_LIB_DEBUG_PATH)
        message(STATUS "Found FBX SDK debug lib: ${FBX_LIB_DEBUG_PATH}")
    endif()
    if(FBX_LIB_RELEASE_PATH)
        message(STATUS "Found FBX SDK release lib: ${FBX_LIB_RELEASE_PATH}")
    endif()

    add_library(FBXSDK_imported UNKNOWN IMPORTED GLOBAL)
    if(FBX_LIB_DEBUG_PATH)
        set_target_properties(FBXSDK_imported PROPERTIES IMPORTED_LOCATION_DEBUG "${FBX_LIB_DEBUG_PATH}")
    endif()
    if(FBX_LIB_RELEASE_PATH)
        set_target_properties(FBXSDK_imported PROPERTIES IMPORTED_LOCATION_RELEASE "${FBX_LIB_RELEASE_PATH}")
    endif()
    # mark found and set link target to the imported target
    set(FBX_FOUND ON)
    set(FBX_LIB_TARGET "FBXSDK_imported")

#    target_include_directories(GLTFConvert PRIVATE "${FBX_SDK_INCLUDE_DIR}")
#    target_link_libraries(GLTFConvert PRIVATE FBXSDK_imported)
#    target_compile_definitions(GLTFConvert PRIVATE USE_FBX)
else()
    # fallback: only search for the plain libfbxsdk or fbxsdk (no md/mt variants)
    find_library(FBX_SDK_LIBRARY
        NAMES libfbxsdk fbxsdk
        HINTS ${FBX_SDK_ROOT}
        PATH_SUFFIXES lib lib/x64 lib/x64/debug lib/x64/release lib/debug lib/release lib/vs2019/x64 lib/vs2017/x64
    )

    if (FBX_SDK_INCLUDE_DIR AND FBX_SDK_LIBRARY)
        message(STATUS "Found FBX SDK: include=${FBX_SDK_INCLUDE_DIR} lib=${FBX_SDK_LIBRARY}")
        target_include_directories(GLTFConvert PRIVATE "${FBX_SDK_INCLUDE_DIR}")
        target_link_libraries(GLTFConvert PRIVATE "${FBX_SDK_LIBRARY}")
    else()
        message(STATUS "FBX SDK not found. To enable FBX support set FBX_SDK_ROOT to your SDK root or install the FBX SDK.")
    endif()
endif()

# If the fallback found a plain FBX SDK library, mark FBX as found and record the library
if (FBX_SDK_INCLUDE_DIR AND FBX_SDK_LIBRARY)
    set(FBX_FOUND ON)
    set(FBX_LIB_TARGET "${FBX_SDK_LIBRARY}")
endif()

if (MSVC)
    if(USE_STATIC_CRT)
        message(STATUS "Using static runtime on MSVC toolchain")
        set_property(TARGET GLTFConvert PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET PureModel PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set_property(TARGET GLTFConvert PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET PureModel PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

target_compile_options(GLTFConvert PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2 /fp:fast /openmp:llvm>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi /Od /RTC1>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /Oi /Ot>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-mavx2 -O3 -ffast-math -fopenmp>
)

target_compile_options(PureModel PRIVATE
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

target_link_libraries(GLTFConvert PRIVATE
    PureModel
    fastgltf::fastgltf
    minipack_writer
)

# Build FBXConvert only if FBX SDK was found
if (FBX_FOUND)
    add_executable(FBXConvert
        fbx/main_fbx.cpp
        TexConv.cpp
        ${FBX_FILES}
        ${FBX_IMPORT_FILES}
    )

    target_compile_definitions(FBXConvert PRIVATE
        GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
        GLM_FORCE_INTRINSICS
    )

    target_link_libraries(FBXConvert PRIVATE
        PureModel
        minipack_writer
    )

    if (FBX_SDK_INCLUDE_DIR)
        target_include_directories(FBXConvert PRIVATE "${FBX_SDK_INCLUDE_DIR}")
    endif()
    if (FBX_LIB_TARGET)
        target_link_libraries(FBXConvert PRIVATE ${FBX_LIB_TARGET})
    endif()
    # Ensure FBX SDK namespace configuration and prevent headers from injecting global using
    # Define FBXSDK_DEFINE_NAMESPACE=1 to match the SDK build (exported symbols are in fbxsdk::)
    target_compile_definitions(FBXConvert PRIVATE FBXSDK_NAMESPACE_USING=1 FBXSDK_DEFINE_NAMESPACE=1)
    
    target_compile_options(FBXConvert PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2 /fp:fast /openmp:llvm>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi /Od /RTC1>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /Oi /Ot>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-mavx2 -O3 -ffast-math -fopenmp>
    )

    if (MSVC)
        if(USE_STATIC_CRT)
            set_property(TARGET FBXConvert PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        else()
            set_property(TARGET FBXConvert PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
        endif()
    endif()
else()
    message(STATUS "FBX SDK not found. FBXConvert will not be built.")
endif()
