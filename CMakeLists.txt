cmake_minimum_required(VERSION 3.16)
project(dsh-plugin-manager VERSION 1.0.0 LANGUAGES CXX)

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
)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(SOURCES
    src/main.cpp
    src/core/PluginManager.cpp
    src/core/TmuxManager.cpp
)

qt_add_executable(dsh-plugin-manager ${SOURCES})

# Theme.qml 是单例
set_source_files_properties(src/ui/qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(dsh-plugin-manager
    URI DshPluginManager
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/Theme.qml
        src/ui/qml/components/PluginList.qml
        src/ui/qml/components/PluginCard.qml
        src/ui/qml/components/TmuxPage.qml
)

target_link_libraries(dsh-plugin-manager PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

set_target_properties(dsh-plugin-manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dsh.plugin-manager"
    MACOSX_BUNDLE_BUNDLE_NAME "DSH Plugin Manager"
)

install(TARGETS dsh-plugin-manager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
