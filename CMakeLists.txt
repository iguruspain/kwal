cmake_minimum_required(VERSION 3.16)

project(kwal
    VERSION 0.1.0
    DESCRIPTION "KDE Plasma wallpaper and style manager"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM 6.31.0 NO_MODULE REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
find_package(KF6 REQUIRED COMPONENTS Config Kirigami)

qt_standard_project_setup(REQUIRES 6.8)

include(KDEInstallDirs)

qt_add_executable(kwal
    src/main.cpp
)

qt_add_qml_module(kwal
    URI org.kde.kwal
    QML_FILES
        qml/Main.qml
)

target_link_libraries(kwal
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        KF6::ConfigCore
        KF6::Kirigami
)

install(FILES org.kde.kwal.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    RENAME kwal.desktop
)
install(FILES icons/scalable/apps/org.kde.kwal.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
