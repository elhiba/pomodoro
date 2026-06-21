#ifndef MEDIA_PLAYER_HPP
#define MEDIA_PLAYER_HPP

#include <QMediaPlayer>
#include <QAudioOutput>

class MediaPlayer
{
	private:
		QMediaPlayer	_player;
		QAudioOutput	_audioOutput;
	public:
		MediaPlayer();
};

#endif
