cmake_minimum_required(VERSION 3.16)

project(pomodoro
		VERSION 1.0.0
		LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SRC main.cpp PomodoroWindow.cpp TimerWidget.cpp assets.qrc)

list(TRANSFORM SRC PREPEND "src/")

add_executable(pomodoro ${SRC})

target_link_libraries(pomodoro PRIVATE Qt6::Widgets)
