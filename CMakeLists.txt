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
# libavutil: already a runtime dependency of Qt Multimedia's FFmpeg backend;
# linked only to set the ffmpeg log level in main.cpp.
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)

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

# material-color-utilities: the official Material You color library. Fetched at
# a pinned commit and built as a static library (no system package exists).
include(FetchContent)
set(MCU_GIT_TAG "bd811e4ca2ab94c4ce3cadc89c19b1820b9704b3")
FetchContent_Declare(
    mcu
    GIT_REPOSITORY https://github.com/material-foundation/material-color-utilities.git
    GIT_TAG ${MCU_GIT_TAG}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(mcu)
# The pinned commit is not self-contained on GCC 16: temperature_cache.h
# uses std::optional without including <optional>, and utils.cc /
# wsmeans.cc pull in abseil (HexFromArgb is only called by mcu's own,
# excluded tests). Patch the checked-out tree once so the static library
# builds standalone.
set(_mcu_root "${mcu_SOURCE_DIR}/cpp")
set(_mcu_temp_cache "${_mcu_root}/temperature/temperature_cache.h")
file(READ "${_mcu_temp_cache}" _mcu_temp_cache_src)
if (NOT _mcu_temp_cache_src MATCHES "#include <optional>")
    string(REPLACE "#include \"cpp/cam/hct.h\""
            "#include \"cpp/cam/hct.h\"\n#include <optional>"
            _mcu_temp_cache_src "${_mcu_temp_cache_src}")
    file(WRITE "${_mcu_temp_cache}" "${_mcu_temp_cache_src}")
endif()
set(_mcu_utils "${_mcu_root}/utils/utils.cc")
file(READ "${_mcu_utils}" _mcu_utils_src)
if (_mcu_utils_src MATCHES "absl/strings/str_cat.h")
    string(REPLACE "#include \"absl/strings/str_cat.h\"" "#include <format>"
            _mcu_utils_src "${_mcu_utils_src}")
    string(REPLACE "return absl::StrCat(absl::Hex(argb));"
            "return std::format(\"{:08x}\", argb);"
            _mcu_utils_src "${_mcu_utils_src}")
    file(WRITE "${_mcu_utils}" "${_mcu_utils_src}")
endif()
set(_mcu_wsmeans "${_mcu_root}/quantize/wsmeans.cc")
file(READ "${_mcu_wsmeans}" _mcu_wsmeans_src)
if (_mcu_wsmeans_src MATCHES "absl/container/flat_hash_map.h")
    string(REPLACE "#include \"absl/container/flat_hash_map.h\"" "#include <unordered_map>"
            _mcu_wsmeans_src "${_mcu_wsmeans_src}")
    string(REPLACE "absl::flat_hash_map" "std::unordered_map"
            _mcu_wsmeans_src "${_mcu_wsmeans_src}")
    file(WRITE "${_mcu_wsmeans}" "${_mcu_wsmeans_src}")
endif()

add_library(mcu_lib STATIC
    ${mcu_SOURCE_DIR}/cpp/blend/blend.cc
    ${mcu_SOURCE_DIR}/cpp/cam/cam.cc
    ${mcu_SOURCE_DIR}/cpp/cam/hct.cc
    ${mcu_SOURCE_DIR}/cpp/cam/hct_solver.cc
    ${mcu_SOURCE_DIR}/cpp/cam/viewing_conditions.cc
    ${mcu_SOURCE_DIR}/cpp/contrast/contrast.cc
    ${mcu_SOURCE_DIR}/cpp/dislike/dislike.cc
    ${mcu_SOURCE_DIR}/cpp/dynamiccolor/dynamic_color.cc
    ${mcu_SOURCE_DIR}/cpp/dynamiccolor/dynamic_scheme.cc
    ${mcu_SOURCE_DIR}/cpp/dynamiccolor/material_dynamic_colors.cc
    ${mcu_SOURCE_DIR}/cpp/palettes/tones.cc
    ${mcu_SOURCE_DIR}/cpp/quantize/celebi.cc
    ${mcu_SOURCE_DIR}/cpp/quantize/lab.cc
    ${mcu_SOURCE_DIR}/cpp/quantize/wsmeans.cc
    ${mcu_SOURCE_DIR}/cpp/quantize/wu.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_content.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_expressive.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_fidelity.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_fruit_salad.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_monochrome.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_neutral.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_rainbow.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_tonal_spot.cc
    ${mcu_SOURCE_DIR}/cpp/scheme/scheme_vibrant.cc
    ${mcu_SOURCE_DIR}/cpp/score/score.cc
    ${mcu_SOURCE_DIR}/cpp/temperature/temperature_cache.cc
    ${mcu_SOURCE_DIR}/cpp/utils/utils.cc
)
target_include_directories(mcu_lib PUBLIC ${mcu_SOURCE_DIR})
target_compile_features(mcu_lib PUBLIC cxx_std_20)

# Wayland client protocols for the wallpaper transition overlay: the
# vendored XMLs plus the Wayland core protocol (libwayland-client no
# longer ships wl_compositor & co. in its headers) are compiled with
# wayland-scanner into a client header (inline request functions,
# listener structs) and a client implementation (the interface structs)
# at build time.
enable_language(C)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER wayland-scanner)
if (NOT WAYLAND_SCANNER)
    message(FATAL_ERROR "wayland-scanner not found")
endif()
set(_kwal_transition_protocols
 "wayland.xml wayland-core"
 "viewporter.xml viewporter"
 "wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1"
 "xdg-output-unstable-v1.xml xdg-output-unstable-v1"
 "xdg-shell.xml xdg-shell"
)
add_library(kwal_transitions STATIC
 src/transitions/effects.cpp
 src/transitions/effects.h
 src/transitions/outputs.cpp
 src/transitions/outputs.h
 src/transitions/overlay.cpp
 src/transitions/overlay.h
 src/transitions/path.cpp
 src/transitions/path.h
 src/transitions/planes.cpp
 src/transitions/planes.h
 src/transitions/transitionspec.cpp
 src/transitions/transitionspec.h
)
target_include_directories(kwal_transitions PUBLIC src/transitions)
target_link_libraries(kwal_transitions
 PUBLIC
 Qt6::Core
 Qt6::Gui
 PkgConfig::WAYLAND_CLIENT
)
set(_kwal_transition_generated)
foreach(_entry IN LISTS _kwal_transition_protocols)
    string(REPLACE " " ";" _parts "${_entry}")
    list(GET _parts 0 _xml)
    list(GET _parts 1 _base)
    set(_xml_path "${CMAKE_CURRENT_SOURCE_DIR}/src/transitions/protocols/${_xml}")
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/transitions/${_base}-client-protocol.h"
    COMMAND ${WAYLAND_SCANNER} client-header "${_xml_path}" "${CMAKE_BINARY_DIR}/transitions/${_base}-client-protocol.h"
    DEPENDS "${_xml_path}"
    COMMENT "Generating ${_base} client header"
)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/transitions/${_base}-client-protocol.c"
        COMMAND ${WAYLAND_SCANNER} private-code "${_xml_path}" "${CMAKE_BINARY_DIR}/transitions/${_base}-client-protocol.c"
        DEPENDS "${_xml_path}"
        COMMENT "Generating ${_base} client protocol"
    )
    list(APPEND _kwal_transition_generated
        "${CMAKE_BINARY_DIR}/transitions/${_base}-client-protocol.h"
        "${CMAKE_BINARY_DIR}/transitions/${_base}-client-protocol.c"
    )
endforeach()
target_sources(kwal_transitions PRIVATE ${_kwal_transition_generated})
target_include_directories(kwal_transitions PUBLIC "${CMAKE_BINARY_DIR}/transitions")

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

qt_add_executable(kwal
 src/appcontroller.cpp
 src/colorcategory.cpp
 src/colorcategory.h
 src/appcontroller.h
 src/fastfetchcontroller.cpp
 src/fastfetchcontroller.h
 src/fdothumbnailprovider.cpp
 src/fdothumbnailprovider.h
 src/paletteextractor.cpp
 src/paletteextractor.h
 src/plasmaapply.cpp
 src/plasmaapply.h
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
 qml/WallpaperDetailDrawer.qml
 qml/CollapsibleSection.qml
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
    PkgConfig::AVUTIL
 mcu_lib
 kwal_transitions
 tomlplusplus::tomlplusplus
)

# Standalone wallpaper transition binary: the app launches it through
# QProcess when a transition is enabled; it can also be run by hand to
# test transitions against the live desktop.
qt_add_executable(kwal-transition
 src/kwal-transition.cpp
 src/plasmaapply.cpp
 src/plasmaapply.h
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/colorcategory.cpp
 src/colorcategory.h
)
target_include_directories(kwal-transition PRIVATE src)
target_link_libraries(kwal-transition
 PRIVATE
 kwal_transitions
 Qt6::Core
 Qt6::Gui
 Qt6::Qml
 Qt6::DBus
 mcu_lib
)

qt_add_executable(tst_transitions
 tests/tst_transitions.cpp
 src/plasmaapply.cpp
 src/plasmaapply.h
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/colorcategory.cpp
 src/colorcategory.h
 src/wallpapermodel.cpp
 src/wallpapermodel.h
)
target_include_directories(tst_transitions PRIVATE src)
target_link_libraries(tst_transitions
 PRIVATE
 kwal_transitions
 Qt6::Core
 Qt6::Gui
 Qt6::Qml
 Qt6::DBus
 Qt6::Test
 mcu_lib
)
add_test(NAME tst_transitions COMMAND tst_transitions)

target_include_directories(kwal PRIVATE src)
qt_add_executable(tst_appcontroller
 tests/tst_appcontroller.cpp
 src/appcontroller.cpp
 src/colorcategory.cpp
 src/colorcategory.h
 src/appcontroller.h
 src/templateinstaller.cpp
 src/templateinstaller.h
 src/wallpapermodel.h
 src/wallpapermodel.cpp
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/paletteextractor.cpp
 src/paletteextractor.h
 src/plasmaapply.cpp
 src/plasmaapply.h
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
 mcu_lib
 KF6::ConfigCore
 kwal_transitions
)
add_test(NAME tst_appcontroller COMMAND tst_appcontroller)
qt_add_executable(tst_wallpapersinfo
 tests/tst_wallpapersinfo.cpp
 src/wallpapersinfo.cpp
 src/colorcategory.cpp
 src/colorcategory.h
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
 mcu_lib
)
add_test(NAME tst_wallpapersinfo COMMAND tst_wallpapersinfo)
qt_add_executable(tst_plasmaapply
 tests/tst_plasmaapply.cpp
 src/plasmaapply.cpp
 src/plasmaapply.h
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/colorcategory.cpp
 src/colorcategory.h
 src/wallpapermodel.cpp
 src/wallpapermodel.h
)
target_include_directories(tst_plasmaapply PRIVATE src)
target_link_libraries(tst_plasmaapply
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::DBus
 Qt6::Qml
 Qt6::Test
 mcu_lib
 kwal_transitions
)
add_test(NAME tst_plasmaapply COMMAND tst_plasmaapply)
qt_add_executable(tst_paletteextractor
 tests/tst_paletteextractor.cpp
 src/paletteextractor.cpp
 src/colorcategory.cpp
 src/colorcategory.h
 src/paletteextractor.h
 src/wallpapersinfo.cpp
 src/wallpapersinfo.h
 src/wallpapermodel.cpp
 src/wallpapermodel.h
)
target_include_directories(tst_paletteextractor PRIVATE src)
target_link_libraries(tst_paletteextractor
 PRIVATE
 Qt6::Core
 Qt6::Gui
 Qt6::Qml
 mcu_lib
 Qt6::Test
)
add_test(NAME tst_paletteextractor COMMAND tst_paletteextractor)
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
