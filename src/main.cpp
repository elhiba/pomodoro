#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>

#include "InstanceBridge.hpp"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

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
	QApplication::setApplicationVersion("1.1.0");
	QApplication::setDesktopFileName("pomodoro");

#ifdef Q_OS_WIN
	// Gives the process a stable identity for the shell. Without one Windows groups the
	// window under the bare executable and the media flyout has no app to name, which is
	// half of why it said "Unknown app"; the other half is the version resource that
	// packaging/pomodoro.rc.in now embeds.
	SetCurrentProcessExplicitAppUserModelID(L"elhiba.pomodoro");
#endif

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

	// Qt Quick Controls picks the platform's native style by default from Qt 6.7 on
	// (Windows/FluentWinUI3 on Windows, macOS on Mac), and a native style refuses every
	// "background:" and "contentItem:" this UI sets -- so the buttons drew their native
	// square behind the custom one, the scroll bars came out oversized, and the icons
	// were tinted with the system palette, turning black under a light Windows theme.
	// Basic honours all of it, so the app looks the same everywhere and owes nothing to
	// the desktop's theme. Must be set before the first QML file is loaded.
	QQuickStyle::setStyle(QStringLiteral("Basic"));

	// Draw on the CPU rather than through Direct3D/OpenGL. This is a handful of
	// rectangles, some text and a few SVG icons -- nothing the GPU pipeline buys
	// anything for -- and going through it costs a 3D context, about sixty megabytes of
	// driver allocations, and enough GPU activity that NVIDIA's overlay mistakes a
	// pomodoro timer for a game. Software rendering is pixel for pixel the same here,
	// measurably cheaper, and works on machines whose graphics drivers are old, broken
	// or virtualised.
	//
	// Skipped when QT_QUICK_BACKEND is already set, so anyone who wants the GPU path
	// back can ask for it without a rebuild.
	if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
		QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

	QQmlApplicationEngine	engine;

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed,
		&pomodoro, []() { QCoreApplication::exit(EXIT_FAILURE); },
		Qt::QueuedConnection
	);

	engine.loadFromModule("Pomodoro", "Main");

	return pomodoro.exec();
}
