cmake_minimum_required(VERSION 3.16)

project(pomodoro
		VERSION 1.0.0
		LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Quick QuickWidgets Multimedia)

set(SRC main.cpp PomodoroWindow.cpp TimerWidget.cpp SettingsWidget.cpp assets.qrc)

list(TRANSFORM SRC PREPEND "src/")

qt_add_executable(pomodoro ${SRC})

qt_import_plugins(pomodoro INCLUDE Qt6::QWaylandIntegrationPlugin Qt6::QXcbIntegrationPlugin)

target_link_libraries(pomodoro PRIVATE Qt6::Widgets Qt6::Quick Qt6::QuickWidgets Qt6::Multimedia)
