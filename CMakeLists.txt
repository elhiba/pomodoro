cmake_minimum_required(VERSION 3.16)
project(pomodoro
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

add_executable(pomodoro
    src/main.cpp
    src/ui/resources.qrc
)

target_link_libraries(pomodoro PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)
