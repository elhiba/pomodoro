#include "TimerWidget.hpp"
#include <QVBoxLayout>

#include <QTimer>

#include <QQuickItem>

TimerWidget::TimerWidget(QWidget *parent) : QWidget(parent), _remainSeconds(_totalSeconds)
{
	_qmlTimerView = new QQuickWidget(this);
	_qmlTimerView->setResizeMode(QQuickWidget::SizeRootObjectToView);
	_qmlTimerView->setSource(QUrl("qrc:/CircularTimerTemplate"));
	_qmlTimerView->setFixedSize(300, 300);

	_countdownTimer = new QTimer(this);
	connect(_countdownTimer, &QTimer::timeout, this, &TimerWidget::updateTimer);
	_countdownTimer->start(1000);

	startButton();

	QVBoxLayout	*timerLayout = new QVBoxLayout(this);

	timerLayout->addStretch();
	timerLayout->addWidget(_qmlTimerView, Qt::AlignCenter, Qt::AlignCenter);
	timerLayout->addWidget(_playButton, Qt::AlignCenter, Qt::AlignCenter);
	timerLayout->addStretch();
}

void	TimerWidget::updateTimer()
{
	if (_remainSeconds > 0 && _start)
	{
		--_remainSeconds;

		int	minutes = _remainSeconds / 60;
		int	seconds = _remainSeconds % 60;

		QString	timeString = QString("%1:%2")
							.arg(minutes, 2, 10, '0')
							.arg(seconds, 2, 10, '0');

		double	progressValue = static_cast<double>(_remainSeconds) / _totalSeconds;

		QObject *root = _qmlTimerView->rootObject();

		if (root)
		{
			root->setProperty("timeText", timeString);
			root->setProperty("progress", progressValue);
		}
	}
	else
	{
		_countdownTimer->stop();
		_start = false;
		_playButton->setIcon(QIcon(":/playTimer"));
	}
}

void	TimerWidget::startButton()
{
	_playButton = new QPushButton(QIcon(":/playTimer"), NULL, this);
	_playButton->setFixedSize(100, 100);
	_playButton->setIconSize(QSize(100, 100));

	connect(_playButton, &QPushButton::clicked, this, [this]
		{
			_start = !_start;

			if (_start)
			{
				_playButton->setIcon(QIcon(":/pauseTimer"));
				_countdownTimer->start(1000);
			}
			else
			{
				_playButton->setIcon(QIcon(":/playTimer"));
				_countdownTimer->stop();
			}
		}
	 );
}
