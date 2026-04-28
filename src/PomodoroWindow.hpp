#ifndef POMODORO_WINDOW_HPP
#define POMODORO_WINDOW_HPP

#include <QApplication>
#include <QWidget>
#include <QPushButton>

class PomodoroWindow : public QWidget
{
	private:
		QPushButton	*_minimizeButton;
		QPushButton	*_maximizeButton;
		QPushButton	*_closeButton;

	public:
		PomodoroWindow();

	protected:
		void	resizeEvent(QResizeEvent *event) override;
		void	designWindow();
};

#endif
