#include <QApplication>
#include "PomodoroWindow.hpp"


int main(int ac, char *av[])
{
	QApplication pomodoro(ac, av);

	PomodoroWindow pomodoroWindow;

	pomodoroWindow.setStyleSheet("border: 1px solid blue; background-color: rgba(0, 0, 0, 0);");

	pomodoroWindow.show();
	return (pomodoro.exec());
}
