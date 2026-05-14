#ifndef TIMER_WIDGET_HPP
#define TIMER_WIDGET_HPP

#include <QWidget>
#include <QLabel>
#include <QQuickWidget>
#include <QPushButton>

class TimerWidget : public QWidget
{

	private slots:
		void	updateTimer();

	private:
		QQuickWidget *_qmlTimerView;
		QTimer	*_countdownTimer;
		int		_totalSeconds = 25 * 60;
		int		_remainSeconds;
		bool	_start = false;

		QPushButton	*_playButton;

		void	startButton();

	public:
		TimerWidget(QWidget *parent);
};

#endif
