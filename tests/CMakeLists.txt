# Only the pure-logic classes are built into the test binary, straight from source.
# Linking the tests against the app target is not an option: qt_add_executable produces
# an executable, not a library, and the QML module resources would come along with it.
find_package(Qt6 6.5 REQUIRED COMPONENTS Test Qml)

qt_add_executable(tst_pomodorotimer
	tst_pomodorotimer.cpp
	../src/core/PomodoroTimer.cpp
)

target_include_directories(tst_pomodorotimer PRIVATE ../src/core)

target_link_libraries(tst_pomodorotimer PRIVATE
	Qt6::Core
	Qt6::Qml
	Qt6::Test
)

add_test(NAME PomodoroTimer COMMAND tst_pomodorotimer)

# The test never opens a window, but QTest still spins up a QCoreApplication; forcing the
# offscreen platform means it also cannot trip over a runner with no display attached.
set_tests_properties(PomodoroTimer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
