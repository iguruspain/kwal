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

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test Widgets)
find_package(KF6 REQUIRED COMPONENTS Config Kirigami)

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

include(KDEInstallDirs)

qt_add_executable(kwal
 src/appcontroller.cpp
 src/appcontroller.h
 src/main.cpp
 src/wallpapermodel.cpp
 src/wallpapermodel.h
)

qt_add_qml_module(kwal
    URI org.kde.kwal
    QML_FILES
        qml/Main.qml
        qml/WallpapersPage.qml
        qml/FolderSidebar.qml
        qml/FolderRow.qml
        qml/WallpaperItem.qml
        qml/PlaceholderPage.qml
)

target_link_libraries(kwal
 PRIVATE
 Qt6::Gui
 Qt6::Quick
 Qt6::QuickControls2
 Qt6::Widgets
 Qt6::DBus
 KF6::ConfigCore
 KF6::Kirigami
)

target_include_directories(kwal PRIVATE src)
qt_add_executable(tst_appcontroller
 tests/tst_appcontroller.cpp
 src/appcontroller.cpp
 src/appcontroller.h
 src/wallpapermodel.cpp
 src/wallpapermodel.h
)
target_include_directories(tst_appcontroller PRIVATE src)
target_link_libraries(tst_appcontroller
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::Qml
 Qt6::DBus
 Qt6::Test
 KF6::ConfigCore
)
add_test(NAME tst_appcontroller COMMAND tst_appcontroller)

install(FILES org.kde.kwal.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    RENAME kwal.desktop
)
install(FILES icons/scalable/apps/org.kde.kwal.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
