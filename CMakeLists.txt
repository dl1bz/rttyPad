cmake_minimum_required(VERSION 3.21)
project(rttyPad VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets WebSockets)

add_library(rtty_core STATIC
    src/Baudot.c
    src/RTTYRx.c
)
target_include_directories(rtty_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(NOT WIN32)
    target_link_libraries(rtty_core PUBLIC m)
endif()

add_executable(rttyPad
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/RTTYScopeWidget.cpp
    src/RTTYScopeWidget.h
    src/TCIClient.cpp
    src/TCIClient.h
    src/NativeRTTYTx.cpp
    src/NativeRTTYTx.h
    Resources/icons.qrc
)
target_link_libraries(rttyPad PRIVATE Qt6::Widgets Qt6::WebSockets rtty_core)
target_compile_definitions(rttyPad PRIVATE RTTYPAD_VERSION=\"${PROJECT_VERSION}\")

# Use the Windows GUI subsystem (no additional console window).
if(WIN32)
    set_target_properties(rttyPad PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
    set_source_files_properties(Resources/AppIcon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(rttyPad PRIVATE Resources/AppIcon.icns)
    set_target_properties(rttyPad PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE AppIcon.icns
        MACOSX_BUNDLE_BUNDLE_NAME "rttyPad")
endif()

# Embed the icon in the Windows executable (Explorer/taskbar).
if(WIN32)
    target_sources(rttyPad PRIVATE Resources/appicon.rc)
endif()
