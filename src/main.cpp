#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int ac, char **av)
{
	QGuiApplication pomodoro(ac, av);
	QQmlApplicationEngine engine;

	const QUrl url(QStringLiteral("qrc:/main.qml"));

	engine.load(url);

    return pomodoro.exec();
}
