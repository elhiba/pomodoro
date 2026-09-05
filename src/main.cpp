#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QStandardPaths>

#include "InstanceBridge.hpp"

static QString	instanceLockPath()
{
	// The runtime directory is per user and cleared on logout, which is exactly the
	// lifetime a "one copy running" lock wants. Temp is the fallback for systems
	// that do not provide one.
	QString	directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

	if (directory.isEmpty())
		directory = QDir::tempPath();

	return directory + QStringLiteral("/pomodoro.lock");
}

int main(int ac, char **av)
{
	// QApplication rather than QGuiApplication: QSystemTrayIcon and QMenu come from Qt
	// Widgets, and there is no Qt Quick equivalent for a system tray entry.
	QApplication	pomodoro(ac, av);

	// The window is hidden to the tray rather than closed, and quitting is explicit
	// (Ctrl+Q, the tray menu, or the settings drawer). Leaving this on would let a
	// hidden window take the application down with it.
	QApplication::setQuitOnLastWindowClosed(false);

	// Set before anything touches QSettings: these decide where the
	// configuration file lands (~/.config/elhiba/pomodoro.conf).
	QApplication::setOrganizationName("elhiba");
	QApplication::setApplicationName("pomodoro");
	QApplication::setApplicationVersion("1.0.0");
	QApplication::setDesktopFileName("pomodoro");

	// What the task switcher and the dock show while the window is minimised. On Wayland
	// the compositor prefers the desktop entry named above, which only resolves once the
	// app is installed, so this is the fallback that makes an uninstalled build look
	// right too.
	QApplication::setWindowIcon(
		QIcon(QStringLiteral(":/qt/qml/Pomodoro/assets/icons/pomodoroLogoTransport.png")));

	// A second copy would fight the first over tasks.json, sessions.json and the settings
	// file, and two timers counting at once is nobody's idea of focus.
	//
	// QLockFile rather than QSharedMemory: a shared memory segment survives a process
	// that is killed or crashes, so one bad exit would leave the app permanently
	// convinced it is already running. QLockFile records the owning pid and reclaims
	// the lock once that process is gone.
	QLockFile	instanceLock(instanceLockPath());

	instanceLock.setStaleLockTime(0);

	if (!instanceLock.tryLock(100))
	{
		// Closing the window only minimises it by default, so the copy already running
		// is very likely sitting in the dock. Bring it back rather than exiting without
		// anything appearing to happen.
		if (InstanceBridge::askRunningInstanceToRaise())
			qInfo("pomodoro is already running, raising the existing window");
		else
			qWarning("pomodoro is already running");

		return EXIT_SUCCESS;
	}

	QQmlApplicationEngine	engine;

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed,
		&pomodoro, []() { QCoreApplication::exit(EXIT_FAILURE); },
		Qt::QueuedConnection
	);

	engine.loadFromModule("Pomodoro", "Main");

	return pomodoro.exec();
}
