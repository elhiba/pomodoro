#include "TimerWidget.hpp"
#include <QVBoxLayout>

TimerWidget::TimerWidget(QWidget *parent) : QWidget(parent)
{
	_timer = new QLabel("25:00", this);
	_timer->setFixedSize(200, 200);
	_timer->setAlignment(Qt::AlignCenter);

	QHBoxLayout	*timerLayout = new QHBoxLayout(this);


	timerLayout->addStretch();
	timerLayout->addWidget(_timer);
	timerLayout->addStretch();
	
}
