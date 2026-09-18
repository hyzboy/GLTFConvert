# GLTFConvertQt —— Qt6 Widgets GUI 外壳。
# 依赖 GLTFConvertCore (C API 动态库); Qt 经 vcpkg toolchain 解析。
# 找不到 Qt 时优雅跳过, 不影响 CLI 构建。

option(GLTFCONVERT_BUILD_GUI "构建 GLTFConvert Qt GUI 外壳" ON)

if(NOT GLTFCONVERT_BUILD_GUI)
    return()
endif()

find_package(Qt6 6.5 COMPONENTS Core Gui Widgets QUIET)

if(NOT Qt6_FOUND)
    message(STATUS "GLTFConvert: Qt6 not found -> GLTFConvertQt skipped (vcpkg install qtbase:x64-windows to enable)")
    return()
endif()

set(CMAKE_AUTOMOC ON)

add_executable(GLTFConvertQt WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ConvertWorker.h
    src/ConvertWorker.cpp
)

target_include_directories(GLTFConvertQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(GLTFConvertQt PRIVATE
    QT_NO_KEYWORDS
    NOMINMAX
)

target_link_libraries(GLTFConvertQt
    PRIVATE
        GLTFConvertCore
        Qt6::Core Qt6::Gui Qt6::Widgets
)

set_target_properties(GLTFConvertQt PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GLTFConvertQt>"
)

if (MSVC)
    if(USE_STATIC_CRT)
        set_property(TARGET GLTFConvertQt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set_property(TARGET GLTFConvertQt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# vcpkg applocal 只拷 bin 下的 DLL,不拷 plugins —— 平台插件自动部署
if(TARGET Qt6::QWindowsIntegrationPlugin)
    add_custom_command(TARGET GLTFConvertQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GLTFConvertQt>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:GLTFConvertQt>/platforms/"
        COMMENT "Deploy Qt platform plugin (qwindows) for GLTFConvertQt"
    )
endif()
