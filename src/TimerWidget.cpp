#include "TimerWidget.hpp"
#include <QVBoxLayout>

TimerWidget::TimerWidget(QWidget *parent) : QWidget(parent)
{
	_timer = new QLabel("25:00", this);
	_timer->setStyleSheet("width: 100px; height: 100px");
	_timer->setAlignment(Qt::AlignCenter);

	QVBoxLayout	*timerLayout = new QVBoxLayout(this);
	timerLayout->addWidget(_timer);
	
}
