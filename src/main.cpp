#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QDebug>
#include "PomodoroWindow.hpp"

int main(int ac, char *av[])
{
	QCoreApplication::setApplicationName("pomodoro");
    QGuiApplication::setDesktopFileName("pomodoro");
	QApplication pomodoro(ac, av);

	QSharedMemory SharedMemory("pomodoro_KeyInstance");

	//if (!SharedMemory.create(1))
	//{
	//	qWarning() << "Pomodoro is already running";

	//	QMessageBox miniBox;

	//	miniBox.setWindowTitle("Pomodoro");
	//	miniBox.setText("Pomodoro Already open!");
	//	QPixmap ico(":/pomodoroLogoTrans");
	//	miniBox.setIconPixmap(ico.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	//	miniBox.setFixedSize(350,150);
	//	miniBox.setWindowFlags(Qt::FramelessWindowHint);
	//	miniBox.exec();
	//	return EXIT_SUCCESS;
	//}

	PomodoroWindow pomodoroWindow;

	pomodoro.setWindowIcon(QIcon(":/pomodoroLogo.png"));

	//pomodoroWindow.setStyleSheet("border: 1px solid red;");

	pomodoroWindow.show();
	return pomodoro.exec();
}
