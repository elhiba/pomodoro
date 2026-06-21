#ifndef TIMER_WIDGET_HPP
#define TIMER_WIDGET_HPP

#include <QWidget>
#include <QLabel>
#include <QQuickWidget>
#include <QPushButton>
#include <QSoundEffect>

#include <QMediaPlayer>
#include <QAudioOutput>

class TimerWidget : public QWidget
{
	Q_OBJECT
	private slots:
		void	updateTimer();

	public slots:
		void	setVolume(double volume);
    	void	setTimerDuration(int minutes, int seconds);
    	void	setStreamUrl(const QString &url);
    	void	setAutoStartTimer(bool autoStart);
    	void	setAutoStartMusic(bool autoStart);

	private:
		QQuickWidget *_qmlTimerView;
		QTimer	*_countdownTimer;
		int		_totalSeconds = 25 * 60;
		int		_remainSeconds;
		bool	_start = false;
		bool	_autoMusic = true;
		
		QUrl	_linkMusic;

		QSoundEffect	_alarmSound;
		QSoundEffect	_clickSound;

		// temp music laucher
		QMediaPlayer	_player;
		QAudioOutput	_audioOutput;
	

		QPushButton	*_playButton;

		void	startButton();

		void	onTimerTimeout();
		void	onStartClick();

	public:
		TimerWidget(QWidget *parent);
};

#endif
