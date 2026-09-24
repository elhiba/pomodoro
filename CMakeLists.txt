cmake_minimum_required(VERSION 3.21)

# The version is the release tag, and nothing else. It used to be typed into this file and
# into main.cpp by hand, which is how v1.2.2 shipped calling itself 1.2.1 -- and then kept
# offering its own release as an update. Now, in order of preference:
#
#   * -DPOMODORO_VERSION=1.2.3, which the release workflow passes from the tag it builds;
#   * the newest v* tag reachable from HEAD, for a local build of a normal checkout;
#   * 0.0.0, for a tarball or a shallow clone with no tags, which is honest about not
#     knowing and will always see a published release as newer.
#
# safe.directory is passed on the command line because the Docker image builds a checkout
# owned by another uid, and git refuses to read those without being told it may.
set(POMODORO_VERSION "" CACHE STRING "Version stamped into the build; defaults to the newest v* git tag")

set(POMODORO_RESOLVED_VERSION "${POMODORO_VERSION}")

if(NOT POMODORO_RESOLVED_VERSION)
	find_program(POMODORO_GIT_EXECUTABLE git)

	if(POMODORO_GIT_EXECUTABLE)
		execute_process(
			COMMAND "${POMODORO_GIT_EXECUTABLE}" -c safe.directory=* describe --tags --match "v[0-9]*" --abbrev=0
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			OUTPUT_VARIABLE POMODORO_RESOLVED_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
	endif()
endif()

# "v1.2.3", "1.2.3" and "v1.2.3-rc1" all come down to the three numbers project() accepts.
if(POMODORO_RESOLVED_VERSION MATCHES "([0-9]+\.[0-9]+\.[0-9]+)")
	set(POMODORO_RESOLVED_VERSION "${CMAKE_MATCH_1}")
else()
	set(POMODORO_RESOLVED_VERSION "0.0.0")
endif()

project(pomodoro
	VERSION ${POMODORO_RESOLVED_VERSION}
	LANGUAGES CXX
)

message(STATUS "pomodoro version ${PROJECT_VERSION}")

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
	Network
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

	src/core/StreamMetadata.hpp
	src/core/StreamMetadata.cpp

	src/core/MediaControls.hpp
	src/core/MediaControls.cpp

	src/core/UpdateChecker.hpp
	src/core/UpdateChecker.cpp

	src/core/TaskList.hpp
	src/core/TaskList.cpp

	src/core/YtDlp.hpp
	src/core/YtDlp.cpp

	src/core/SpotifyClient.hpp
	src/core/SpotifyClient.cpp
)

# The OS media-control bridge is the one part of the app with a different body per
# platform. Exactly one of these goes in; MediaControls::create() in the common file
# is the only thing that knows which.
if(WIN32)
	target_sources(pomodoro PRIVATE
		src/core/MediaControlsWindows.cpp
		src/core/ShellIdentity.hpp
		src/core/ShellIdentityWindows.cpp
	)

	# runtimeobject: RoGetActivationFactory and the HSTRING helpers. shell32/ole32: the
	# AppUserModelID and the Start menu shortcut. propsys/shlwapi: the property store the
	# shortcut carries its id in.
	target_link_libraries(pomodoro PRIVATE runtimeobject shell32 ole32 propsys shlwapi)

	# A GUI subsystem executable. Without this the linker builds a console program and
	# Windows opens a terminal window behind the app every time it is launched.
	set_target_properties(pomodoro PROPERTIES WIN32_EXECUTABLE TRUE)

	# The .exe icon. Configured rather than used directly so the path inside is absolute
	# and resolves the same way under rc.exe and windres.
	enable_language(RC)
	set(POMODORO_WINDOWS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/pomodoro.ico")

	# VERSIONINFO wants the version as a comma-separated quad, not dotted.
	set(POMODORO_VERSION_COMMA "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
	configure_file(packaging/pomodoro.rc.in "${CMAKE_CURRENT_BINARY_DIR}/pomodoro.rc" @ONLY)
	target_sources(pomodoro PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/pomodoro.rc")
elseif(APPLE)
	enable_language(OBJCXX)
	target_sources(pomodoro PRIVATE src/core/MediaControlsMac.mm)
	target_link_libraries(pomodoro PRIVATE "-framework MediaPlayer" "-framework AppKit")

	set_target_properties(pomodoro PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_BUNDLE_NAME "Pomodoro"
		MACOSX_BUNDLE_GUI_IDENTIFIER "org.elhiba.pomodoro"
		MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
	)

	# The .icns is generated on the macOS release runner from the PNG (sips + iconutil),
	# so it is optional here: a checkout without it still builds, just with no dock icon
	# until the app sets one at runtime.
	set(POMODORO_MAC_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/pomodoro.icns")

	if(EXISTS "${POMODORO_MAC_ICON}")
		target_sources(pomodoro PRIVATE "${POMODORO_MAC_ICON}")
		set_source_files_properties("${POMODORO_MAC_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
		set_target_properties(pomodoro PROPERTIES MACOSX_BUNDLE_ICON_FILE pomodoro.icns)
	endif()
else()
	target_sources(pomodoro PRIVATE
		src/core/MediaControlsMpris.cpp
		src/core/ShellIdentity.hpp
		src/core/DesktopIntegrationLinux.cpp
	)
endif()

# main.cpp hands this to QApplication, which is where the About section and the update
# check read it from.
target_compile_definitions(pomodoro PRIVATE POMODORO_VERSION_STRING="${PROJECT_VERSION}")

# The Spotify app this build signs in as. A Client ID is not a secret -- PKCE needs none --
# so it can be passed in from a repository variable at release time. Empty means every user
# enters their own in the settings.
set(POMODORO_SPOTIFY_CLIENT_ID "" CACHE STRING "Spotify Client ID built into the app (optional)")
target_compile_definitions(pomodoro PRIVATE POMODORO_SPOTIFY_CLIENT_ID="${POMODORO_SPOTIFY_CLIENT_ID}")

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
	src/ui/MiniTimer.qml
	src/ui/TasksPanel.qml
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
	Qt6::Network
	Qt6::Svg
	Qt6::DBus
	Qt6::Widgets
)

# The desktop entry has to be named after setDesktopFileName("pomodoro") in main.cpp,
# which is also what tells the notification daemon which icon to show.
include(GNUInstallDirs)

install(TARGETS pomodoro
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	BUNDLE DESTINATION .
)

install(FILES packaging/pomodoro.desktop
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES assets/icons/pomodoroLogoTransport.png
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/500x500/apps
	RENAME pomodoro.png
)

# Off by default so a plain configure does not need Qt Test. CI turns it on. Declared
# before include(CTest), which would otherwise default it to ON.
option(BUILD_TESTING "Build the unit tests (needs Qt Test)" OFF)
include(CTest)

if(BUILD_TESTING)
	add_subdirectory(tests)
endif()
