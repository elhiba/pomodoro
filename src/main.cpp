#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int ac, char **av)
{
	QGuiApplication	pomodoro(ac, av);

	// Set before anything touches QSettings: these decide where the
	// configuration file lands (~/.config/elhiba/pomodoro.conf).
	QGuiApplication::setOrganizationName("elhiba");
	QGuiApplication::setApplicationName("pomodoro");
	QGuiApplication::setApplicationVersion("1.0.0");
	QGuiApplication::setDesktopFileName("pomodoro");

	QQmlApplicationEngine	engine;

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed,
		&pomodoro, []() { QCoreApplication::exit(EXIT_FAILURE); },
		Qt::QueuedConnection
	);

	engine.loadFromModule("Pomodoro", "Main");

	return pomodoro.exec();
}
