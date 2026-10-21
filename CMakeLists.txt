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

include(KDEInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Multimedia Qml Quick QuickControls2 Test Widgets WebEngineQuick)
find_package(KF6 REQUIRED COMPONENTS Config Kirigami KirigamiPlatform)

# KF6 6.31's installed CMake package creates imported targets for the Kirigami
# backing libraries and registers the QML plugin target names, but never
# creates the plugin targets themselves. Qt 6.12's target finalizer then warns
# once per missing plugin on every clean configure. Point the registered names
# at the installed plugin libraries.
find_path(Kwal_KirigamiQmlDir NAMES org/kde/kirigami/qmldir
    HINTS /usr/lib/qt6/qml /usr/lib/qml
)
if (NOT Kwal_KirigamiQmlDir)
    message(FATAL_ERROR "Kirigami QML directory not found")
endif()
set(_kwal_kirigami_qml_plugins
    "KF6Kirigamiplugin libKirigamiplugin.so"
    "KF6KirigamiControlsplugin controls/libKirigamiControlsplugin.so"
    "KF6KirigamiDelegates delegates/libKirigamiDelegatesplugin.so"
    "KF6KirigamiDialogsplugin dialogs/libKirigamiDialogsplugin.so"
    "KF6KirigamiFormsplugin forms/libKirigamiFormsplugin.so"
    "KF6KirigamiFormsPrivateCardsplugin forms/private/cards/libKirigamiFormsPrivateCardsplugin.so"
    "KF6KirigamiFormsPrivateFlatplugin forms/private/flat/libKirigamiFormsPrivateFlatplugin.so"
    "KF6KirigamiFormsPrivateTemplatesplugin forms/private/templates/libKirigamiFormsPrivateTemplatesplugin.so"
    "KF6KirigamiLayoutsplugin layouts/libKirigamiLayoutsplugin.so"
    "KF6KirigamiLayoutsPrivateplugin layouts/private/libKirigamiLayoutsPrivateplugin.so"
    "KF6KirigamiPrivateplugin private/libKirigamiPrivateplugin.so"
    "KF6KirigamiPolyfillplugin private/polyfill/libKirigamiPolyfillplugin.so"
    "KF6KirigamiPrimitives primitives/libKirigamiPrimitivesplugin.so"
    "KF6KirigamiTemplates templates/libKirigamiTemplatesplugin.so"
    "KF6::KirigamiPlatformplugin platform/libKirigamiPlatformplugin.so"
)
foreach(_entry IN LISTS _kwal_kirigami_qml_plugins)
    string(REPLACE " " ";" _parts "${_entry}")
    list(GET _parts 0 _plugin_target)
    list(GET _parts 1 _plugin_lib)
    if (NOT TARGET ${_plugin_target})
        add_library(${_plugin_target} SHARED IMPORTED)
        set_target_properties(${_plugin_target} PROPERTIES
            IMPORTED_LOCATION "${Kwal_KirigamiQmlDir}/${_plugin_lib}"
        )
    endif()
endforeach()
# toml++ (header-only): prefer the system package, fall back to a pinned release.
find_package(tomlplusplus 3.4 QUIET CONFIG)
if (NOT tomlplusplus_FOUND)
    include(FetchContent)
    FetchContent_Declare(tomlplusplus
        URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
        URL_HASH SHA256=8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(tomlplusplus)
endif()

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

qt_add_executable(kwal
 src/appcontroller.cpp
 src/appcontroller.h
 src/fastfetchcontroller.cpp
 src/fastfetchcontroller.h
 src/fdothumbnailprovider.cpp
 src/fdothumbnailprovider.h
 src/main.cpp
 src/starshipcontroller.cpp
 src/starshipcontroller.h
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/templateinstaller.cpp
 src/templateinstaller.h
 src/tools/fileutil.cpp
 src/tools/fileutil.h
 src/tools/fastfetchconfig.cpp
 src/tools/fastfetchconfig.h
 src/tools/starshipconfig.cpp
 src/tools/starshipconfig.h
 src/tools/starshippreview.cpp
 src/tools/starshippreview.h
 src/tools/tintimage.cpp
 src/tools/tintimage.h
 src/tools/ulaunchertheme.cpp
 src/tools/ulaunchertheme.h
 src/tools/ulauncherpreview.cpp
 src/tools/ulauncherpreview.h
 src/ulaunchercontroller.cpp
 src/ulaunchercontroller.h
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
 qml/SettingsWindow.qml
 qml/GeneralSettingsPage.qml
 qml/SettingsPlaceholderPage.qml
 qml/FastfetchPage.qml
 qml/StarshipPage.qml
 qml/StarshipPreview.qml
 qml/UlauncherPage.qml
 qml/UlauncherSettingsPage.qml
 qml/UlauncherMockup.qml
    qml/WallpapersSettingsPage.qml
    qml/ColorDot.qml
    qml/ColorRow.qml
 RESOURCES
 resources/resources.qrc
)

target_link_libraries(kwal
 PRIVATE
 Qt6::Core
 Qt6::DBus
 Qt6::Gui
 Qt6::Multimedia
 Qt6::Widgets
Qt6::WebEngineQuick
 KF6::ConfigCore
 KF6::Kirigami
 tomlplusplus::tomlplusplus
)

target_include_directories(kwal PRIVATE src)
qt_add_executable(tst_appcontroller
 tests/tst_appcontroller.cpp
 src/appcontroller.cpp
 src/appcontroller.h
 src/templateinstaller.cpp
 src/templateinstaller.h
 src/wallpapermodel.h
 src/wallpapermodel.cpp
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/ulaunchercontroller.cpp
 src/ulaunchercontroller.h
 src/tools/fileutil.cpp
 src/tools/fileutil.h
 src/tools/ulaunchertheme.cpp
 src/tools/ulaunchertheme.h
 src/tools/ulauncherpreview.cpp
 src/tools/ulauncherpreview.h
)
add_library(kwal_resources OBJECT)
qt_add_resources(KWAL_RESOURCES_RCC resources/resources.qrc)
target_sources(kwal_resources PRIVATE ${KWAL_RESOURCES_RCC})
target_link_libraries(kwal_resources PRIVATE Qt6::Core)
target_include_directories(tst_appcontroller PRIVATE src)
target_sources(tst_appcontroller PRIVATE $<TARGET_OBJECTS:kwal_resources>)
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
qt_add_executable(tst_wallpapersinfo
 tests/tst_wallpapersinfo.cpp
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/wallpapermodel.cpp
 src/wallpapermodel.h
)
target_include_directories(tst_wallpapersinfo PRIVATE src)
target_link_libraries(tst_wallpapersinfo
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::Qml
 Qt6::Test
)
add_test(NAME tst_wallpapersinfo COMMAND tst_wallpapersinfo)
qt_add_executable(tst_tools
 tests/tst_tools.cpp
 src/tools/fileutil.cpp
 src/tools/fileutil.h
 src/tools/fastfetchconfig.cpp
 src/tools/fastfetchconfig.h
 src/tools/tintimage.cpp
 src/tools/tintimage.h
 src/templateinstaller.cpp
 src/templateinstaller.h
)
target_include_directories(tst_tools PRIVATE src)
target_sources(tst_tools PRIVATE $<TARGET_OBJECTS:kwal_resources>)
target_link_libraries(tst_tools
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::Test
)
add_test(NAME tst_tools COMMAND tst_tools)
qt_add_executable(tst_fastfetch
 tests/tst_fastfetch.cpp
 src/fastfetchcontroller.cpp
 src/fastfetchcontroller.h
 src/tools/fileutil.cpp
 src/tools/fileutil.h
 src/tools/fastfetchconfig.cpp
 src/tools/fastfetchconfig.h
 src/tools/tintimage.cpp
 src/tools/tintimage.h
)
target_include_directories(tst_fastfetch PRIVATE src)
target_link_libraries(tst_fastfetch
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::Test
)
add_test(NAME tst_fastfetch COMMAND tst_fastfetch)


qt_add_executable(tst_starship
 tests/tst_starship.cpp
 src/tools/fileutil.cpp
 src/tools/fileutil.h
 src/tools/starshipconfig.cpp
 src/tools/starshipconfig.h
 src/tools/starshippreview.cpp
 src/tools/starshippreview.h
 src/starshipcontroller.cpp
 src/starshipcontroller.h
)
target_include_directories(tst_starship PRIVATE src)
target_link_libraries(tst_starship
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::Qml
 Qt6::Test
 tomlplusplus::tomlplusplus
)
add_test(NAME tst_starship COMMAND tst_starship)

qt_add_executable(tst_ulauncher
 tests/tst_ulauncher.cpp
 src/tools/fileutil.cpp
 src/tools/fileutil.h
 src/tools/ulaunchertheme.cpp
 src/tools/ulaunchertheme.h
 src/tools/ulauncherpreview.cpp
 src/tools/ulauncherpreview.h
 src/ulaunchercontroller.cpp
 src/ulaunchercontroller.h
 src/templateinstaller.cpp
 src/templateinstaller.h
)
target_include_directories(tst_ulauncher PRIVATE src)
target_link_libraries(tst_ulauncher
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::Qml
 Qt6::Test
 KF6::ConfigCore
)
add_test(NAME tst_ulauncher COMMAND tst_ulauncher)

install(FILES org.kde.kwal.desktop
 DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
 RENAME kwal.desktop
)
install(FILES icons/scalable/apps/org.kde.kwal.svg
 DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
