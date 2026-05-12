#ifndef POMODORO_WINDOW_HPP
#define POMODORO_WINDOW_HPP

#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

class PomodoroWindow : public QWidget
{
	private:
		QPushButton	*_minimizeButton;
		QPushButton	*_maximizeButton;
		QPushButton	*_closeButton;

		QFont		_PlaywriteFont;

	public:
		PomodoroWindow();

		void	assetsLoader();
		void	execute();

	protected:
		void	changeEvent(QEvent *event) override;
		void	designWindow();
		void	mousePressEvent();
};

#endif
