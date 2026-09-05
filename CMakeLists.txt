cmake_minimum_required(VERSION 3.21)

project(pomodoro
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
	Core
	Gui
	Qml
	Quick
	QuickControls2
	Multimedia
	Svg
	DBus
	Widgets
)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(pomodoro
	src/main.cpp

	src/core/PomodoroTimer.hpp
	src/core/PomodoroTimer.cpp

	src/core/AppSettings.hpp
	src/core/AppSettings.cpp

	src/core/SoundPlayer.hpp
	src/core/SoundPlayer.cpp

	src/core/SessionLog.hpp
	src/core/SessionLog.cpp

	src/core/MusicPlayer.hpp
	src/core/MusicPlayer.cpp

	src/core/Notifier.hpp
	src/core/Notifier.cpp

	src/core/InstanceBridge.hpp
	src/core/InstanceBridge.cpp

	src/core/TrayIcon.hpp
	src/core/TrayIcon.cpp
)

# qmltyperegistrar generates #include "PomodoroTimer.hpp" for the QML_ELEMENT types,
# so src/core has to be on the include path.
target_include_directories(pomodoro PRIVATE src/core)

# The QML sources live under src/ui/, but a QML type name is taken from the file's
# path inside the module. Alias each one to the module root so that they register
# as Main, TopMenu and TimerDisplay rather than src/ui/Main and friends.
set(POMODORO_QML_FILES
	src/ui/Main.qml
	src/ui/TopMenu.qml
	src/ui/TimerDisplay.qml
	src/ui/ModeTabs.qml
	src/ui/SettingsPanel.qml
	src/ui/NumberSetting.qml
	src/ui/ToggleSetting.qml
	src/ui/SliderSetting.qml
	src/ui/StatsPanel.qml
	src/ui/ResizeHandles.qml
)

foreach(qml_file IN LISTS POMODORO_QML_FILES)
	cmake_path(GET qml_file FILENAME qml_name)
	set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

# Against a shared Qt build (the system Qt here) the QML import scan cannot link
# anything -- the plugin targets such as Qt6::qtquick2plugin only exist in a static
# build -- so it just emits a warning per plugin. Skip it, but keep it enabled if
# this is ever built against a static Qt, where the scan is what links the plugins in.
get_target_property(POMODORO_QT_CORE_TYPE Qt6::Core TYPE)

if(POMODORO_QT_CORE_TYPE STREQUAL "SHARED_LIBRARY")
	set(POMODORO_IMPORT_SCAN NO_IMPORT_SCAN)
else()
	set(POMODORO_IMPORT_SCAN "")
endif()

qt_add_qml_module(pomodoro
	URI Pomodoro
	VERSION 1.0
	${POMODORO_IMPORT_SCAN}
	QML_FILES ${POMODORO_QML_FILES}
	RESOURCES
		assets/fonts/PlaywriteAR.ttf
		assets/fonts/JetBrainsMono.ttf

		assets/icons/close.svg
		assets/icons/minimize.svg
		assets/icons/maximize.svg
		assets/icons/maximizeReverse.svg
		assets/icons/menu.svg
		assets/icons/playTimer.svg
		assets/icons/pauseTimer.svg
		assets/icons/restartTimer.svg
		assets/icons/pomodoroLogo.png
		assets/icons/pomodoroLogoTransport.png

		assets/sounds/alarmWood.wav
		assets/sounds/button.wav
)

target_link_libraries(pomodoro PRIVATE
	Qt6::Core
	Qt6::Gui
	Qt6::Qml
	Qt6::Quick
	Qt6::QuickControls2
	Qt6::Multimedia
	Qt6::Svg
	Qt6::DBus
	Qt6::Widgets
)

# The desktop entry has to be named after setDesktopFileName("pomodoro") in main.cpp,
# which is also what tells the notification daemon which icon to show.
include(GNUInstallDirs)

install(TARGETS pomodoro
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/pomodoro.desktop
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES assets/icons/pomodoroLogoTransport.png
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/500x500/apps
	RENAME pomodoro.png
)
