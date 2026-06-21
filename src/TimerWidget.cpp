#include "TimerWidget.hpp"
#include <QVBoxLayout>

#include <QTimer>

#include <QQuickItem>

TimerWidget::TimerWidget(QWidget *parent) : QWidget(parent), _remainSeconds(_totalSeconds)
{
	// load timer interface
	_qmlTimerView = new QQuickWidget(this);
	_qmlTimerView->setResizeMode(QQuickWidget::SizeRootObjectToView);
	_qmlTimerView->setSource(QUrl("qrc:/CircularTimerTemplate"));
	_qmlTimerView->setFixedSize(300, 300);

	// timer logic
	_countdownTimer = new QTimer(this);
	connect(_countdownTimer, &QTimer::timeout, this, &TimerWidget::updateTimer);
	//_countdownTimer->start(1000);

	// load start button logic & graphic
	startButton();

	// playing stream music (lofi)
	_player.setAudioOutput(&_audioOutput);
	_audioOutput.setVolume(0.5);

	_player.setSource(QUrl("https://stream.zeno.fm/f3wvbbqmdg8uv"));
	if (_autoMusic)
		_player.play();

	// load sound
	_alarmSound.setSource(QUrl("qrc:/alarmDigital"));
	_alarmSound.setLoopCount(3);
	//connect(_countdownTimer, &QTimer::timeout, this, &TimerWidget::onTimerTimeout);

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
	else if (_remainSeconds == 0)
	{
		_alarmSound.play();
		_countdownTimer->stop();
		_playButton->setIcon(QIcon(":/restartTimer"));
		_start = false;

		connect(_playButton, &QPushButton::clicked, this, [this](){ _remainSeconds = _totalSeconds;});
	}
}

void	TimerWidget::startButton()
{
	_playButton = new QPushButton(QIcon(":/playTimer"), NULL, this);
	_playButton->setStyleSheet("QPushButton { color: #FFFFFF; background: transparent; border: none; }");
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

void	TimerWidget::onTimerTimeout()
{
	_alarmSound.play();
}

void	TimerWidget::onStartClick()
{
	_clickSound.play();
}

void	TimerWidget::setVolume(double volume)
{
	_audioOutput.setVolume(volume);
}

void	TimerWidget::setTimerDuration(int minutes, int seconds)
{
	//qDebug() << "MENU CHANGED TIME TO:" << minutes << "min" << seconds << "sec";
	if (!_start) {
	        _totalSeconds = (minutes * 60) + seconds;
	        _remainSeconds = _totalSeconds;
	
	        // Instantly update the QML circle timer UI to show the new time
	        QString timeString = QString("%1:%2")
	                                .arg(minutes, 2, 10, QChar('0'))
	                                .arg(seconds, 2, 10, QChar('0'));
	
	        QObject *root = _qmlTimerView->rootObject();
	        if (root) {
	            root->setProperty("timeText", timeString);
	            root->setProperty("progress", 1.0); // Reset circle to 100%
	        }
	    }
}

void	TimerWidget::setStreamUrl(const QString &url)
{
	qDebug() << "New url has been set: " << url;
	_linkMusic = url;
	_player.setSource(_linkMusic);
	_player.play();
}

void	TimerWidget::setAutoStartTimer(bool autoStart)
{
}

void	TimerWidget::setAutoStartMusic(bool autoStart)
{
	_autoMusic = autoStart;
	qDebug() << "Music: " << _autoMusic;
}
