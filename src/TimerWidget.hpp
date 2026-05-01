#ifndef TIMER_WIDGET_HPP
#define TIMER_WIDGET_HPP

#include <QWidget>
#include <QLabel>

class TimerWidget : public QWidget
{
	private:
		QLabel	*_timer;
	public:
		TimerWidget(QWidget *parent);
};

#endif
