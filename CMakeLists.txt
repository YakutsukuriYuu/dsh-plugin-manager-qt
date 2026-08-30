cmake_minimum_required(VERSION 3.16)
project(dsh-plugin-manager VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 路径（macOS 官方安装器默认位置）
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.2/macos")
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    QuickControls2
    Widgets
    Svg
    Network
    Concurrent
)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(SOURCES
    src/main.cpp
    src/core/PluginManager.cpp
    src/core/TmuxManager.cpp
    src/core/UpdateChecker.cpp
    src/core/RemoteManager.cpp
    src/core/SyncManager.cpp
)

qt_add_executable(dsh-plugin-manager ${SOURCES})

# 应用版本号传入代码（QCoreApplication::applicationVersion）
target_compile_definitions(dsh-plugin-manager PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

# macOS 应用图标（打进 .app 包的 Resources 目录）
set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")
target_sources(dsh-plugin-manager PRIVATE ${APP_ICON_MACOS})

# Liquid Glass 图标（.icon 的 actool 预编译产物，随仓库提交，
# CI 无需 Xcode 26+；本地如需修改图标，用 Icon Composer 编辑
# resources/AppIcon.icon 后重新编译替换 Assets.car）
set(APP_ASSETS_CAR "${CMAKE_CURRENT_SOURCE_DIR}/resources/Assets.car")
set_source_files_properties(${APP_ASSETS_CAR} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")
target_sources(dsh-plugin-manager PRIVATE ${APP_ASSETS_CAR})

# Theme.qml 是单例
set_source_files_properties(src/ui/qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(dsh-plugin-manager
    URI DshPluginManager
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/Theme.qml
        src/ui/qml/components/AppIcon.qml
        src/ui/qml/components/PluginList.qml
        src/ui/qml/components/PluginCard.qml
        src/ui/qml/components/TmuxPage.qml
        src/ui/qml/components/RemotePage.qml
        src/ui/qml/components/SyncDialog.qml
)

target_link_libraries(dsh-plugin-manager PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    Qt6::Concurrent
)

# 界面图片资源（侧边栏 Logo）
qt_add_resources(dsh-plugin-manager "images"
    PREFIX "/images"
    BASE "resources"
    FILES resources/logo.png
)

# Lucide 图标资源（tools/make_icons.py 预烘焙的着色变体）
include(resources/icons/icons.cmake)
qt_add_resources(dsh-plugin-manager "icons"
    PREFIX "/icons"
    BASE "resources/icons"
    FILES ${ICON_FILES}
)

set_target_properties(dsh-plugin-manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dsh.plugin-manager"
    MACOSX_BUNDLE_BUNDLE_NAME "DSH Plugin Manager"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

install(TARGETS dsh-plugin-manager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
