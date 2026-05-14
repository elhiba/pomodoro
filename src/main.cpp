#include <QApplication>
#include "PomodoroWindow.hpp"

int main(int ac, char *av[])
{
	QApplication pomodoro(ac, av);

	PomodoroWindow pomodoroWindow;

	pomodoro.setWindowIcon(QIcon(":/pomodoroLogo.png"));

	//pomodoroWindow.setStyleSheet("border: 1px solid red;");

	pomodoroWindow.show();
	return (pomodoro.exec());
}
