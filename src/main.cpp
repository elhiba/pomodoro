#include <QApplication>
#include "PomodoroWindow.hpp"


int main(int ac, char *av[])
{
	QApplication pomodoro(ac, av);

	PomodoroWindow window;

	window.show();
	return (pomodoro.exec());
}
