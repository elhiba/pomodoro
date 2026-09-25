#include "AppSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
	const char *const	KeyFocusMinutes = "timer/focusMinutes";
	const char *const	KeyShortBreakMinutes = "timer/shortBreakMinutes";
	const char *const	KeyLongBreakMinutes = "timer/longBreakMinutes";
	const char *const	KeyRoundsBeforeLongBreak = "timer/roundsBeforeLongBreak";

	const char *const	KeyAutoStartBreaks = "behaviour/autoStartBreaks";
	const char *const	KeyAutoStartFocus = "behaviour/autoStartFocus";

	const char *const	KeyAlarmVolume = "sound/alarmVolume";

	const char *const	KeyStreamUrl = "music/streamUrl";
	const char *const	KeyMusicVolume = "music/volume";
	const char *const	KeyMusicFollowsFocus = "music/followsFocus";

	const char *const	KeyAlwaysOnTop = "window/alwaysOnTop";
	const char *const	KeyCloseMinimizes = "window/closeMinimizes";
	const char *const	KeyMiniTimer = "window/miniTimer";
	const char *const	KeyMiniTimerX = "window/miniTimerX";
	const char *const	KeyMiniTimerY = "window/miniTimerY";
	const char *const	KeyTasksEnabled = "tasks/enabled";
	const char *const	KeyMusicSource = "music/source";
	const char *const	KeyRadioUrl = "music/radioUrl";
	const char *const	KeyYoutubeUrl = "music/youtubeUrl";
	const char *const	KeySpotifyUri = "music/spotifyUri";
	const char *const	KeySpotifyClientId = "spotify/clientId";
	const char *const	KeyCustomStations = "music/customStations";
	const char *const	KeySpotifySaved = "music/spotifySaved";
	const char *const	KeyFocusColor = "appearance/focusColor";
	const char *const	KeyShortBreakColor = "appearance/shortBreakColor";
	const char *const	KeyLongBreakColor = "appearance/longBreakColor";
	const char *const	KeyTimerStyle = "appearance/timerStyle";
	const char *const	KeyFontOnTitle = "appearance/fontOnTitle";
	const char *const	KeyAppFont = "appearance/font";
}

const QStringList	&AppSettings::musicSources()
{
	static const QStringList	sources = {
		QStringLiteral("radio"), QStringLiteral("youtube"), QStringLiteral("spotify")
	};

	return sources;
}

const QString	&AppSettings::defaultYoutubeUrl()
{
	// Lofi Girl's channel, which always points at whichever stream it has live.
	static const QString	url = QStringLiteral("https://www.youtube.com/@LofiGirl/live");

	return url;
}

const QString	&AppSettings::defaultStreamUrl()
{
	static const QString	url = QStringLiteral("https://stream.zeno.fm/f3wvbbqmdg8uv");

	return url;
}

AppSettings::AppSettings(QObject *parent)
	: QObject(parent)
{
	load();
}

AppSettings::~AppSettings()
{
	// QSettings flushes on destruction anyway, but being explicit means the file is
	// on disk before the rest of the shutdown runs.
	_store.sync();
}

int	AppSettings::focusMinutes() const
{
	return _focusMinutes;
}

int	AppSettings::shortBreakMinutes() const
{
	return _shortBreakMinutes;
}

int	AppSettings::longBreakMinutes() const
{
	return _longBreakMinutes;
}

int	AppSettings::roundsBeforeLongBreak() const
{
	return _roundsBeforeLongBreak;
}

bool	AppSettings::autoStartBreaks() const
{
	return _autoStartBreaks;
}

bool	AppSettings::autoStartFocus() const
{
	return _autoStartFocus;
}

qreal	AppSettings::alarmVolume() const
{
	return _alarmVolume;
}

QString	AppSettings::streamUrl() const
{
	return _streamUrl;
}

qreal	AppSettings::musicVolume() const
{
	return _musicVolume;
}

bool	AppSettings::musicFollowsFocus() const
{
	return _musicFollowsFocus;
}

bool	AppSettings::alwaysOnTop() const
{
	return _alwaysOnTop;
}

bool	AppSettings::closeMinimizes() const
{
	return _closeMinimizes;
}

bool	AppSettings::miniTimer() const
{
	return _miniTimer;
}

int	AppSettings::miniTimerX() const
{
	return _miniTimerX;
}

int	AppSettings::miniTimerY() const
{
	return _miniTimerY;
}

bool	AppSettings::tasksEnabled() const
{
	return _tasksEnabled;
}

QString	AppSettings::musicSource() const
{
	return _musicSource;
}

QString	AppSettings::radioUrl() const
{
	return _radioUrl;
}

QString	AppSettings::youtubeUrl() const
{
	return _youtubeUrl;
}

QString	AppSettings::spotifyUri() const
{
	return _spotifyUri;
}

QString	AppSettings::spotifyClientId() const
{
	return _spotifyClientId;
}

QString	AppSettings::customStations() const
{
	return _customStations;
}

QString	AppSettings::spotifySaved() const
{
	return _spotifySaved;
}

QString	AppSettings::focusColor() const
{
	return _focusColor;
}

QString	AppSettings::shortBreakColor() const
{
	return _shortBreakColor;
}

QString	AppSettings::longBreakColor() const
{
	return _longBreakColor;
}

QString	AppSettings::timerStyle() const
{
	return _timerStyle;
}

QString	AppSettings::appFont() const
{
	return _appFont;
}

bool	AppSettings::fontOnTitle() const
{
	return _fontOnTitle;
}

int	AppSettings::minimumMinutes() const
{
	return MinimumMinutes;
}

int	AppSettings::maximumMinutes() const
{
	return MaximumMinutes;
}

int	AppSettings::minimumRounds() const
{
	return MinimumRounds;
}

int	AppSettings::maximumRounds() const
{
	return MaximumRounds;
}

void	AppSettings::setFocusMinutes(int minutes)
{
	minutes = qBound(MinimumMinutes, minutes, MaximumMinutes);

	if (_focusMinutes == minutes)
		return;

	_focusMinutes = minutes;
	store(KeyFocusMinutes, minutes);

	emit focusMinutesChanged();
}

void	AppSettings::setShortBreakMinutes(int minutes)
{
	minutes = qBound(MinimumMinutes, minutes, MaximumMinutes);

	if (_shortBreakMinutes == minutes)
		return;

	_shortBreakMinutes = minutes;
	store(KeyShortBreakMinutes, minutes);

	emit shortBreakMinutesChanged();
}

void	AppSettings::setLongBreakMinutes(int minutes)
{
	minutes = qBound(MinimumMinutes, minutes, MaximumMinutes);

	if (_longBreakMinutes == minutes)
		return;

	_longBreakMinutes = minutes;
	store(KeyLongBreakMinutes, minutes);

	emit longBreakMinutesChanged();
}

void	AppSettings::setRoundsBeforeLongBreak(int rounds)
{
	rounds = qBound(MinimumRounds, rounds, MaximumRounds);

	if (_roundsBeforeLongBreak == rounds)
		return;

	_roundsBeforeLongBreak = rounds;
	store(KeyRoundsBeforeLongBreak, rounds);

	emit roundsBeforeLongBreakChanged();
}

void	AppSettings::setAutoStartBreaks(bool autoStart)
{
	if (_autoStartBreaks == autoStart)
		return;

	_autoStartBreaks = autoStart;
	store(KeyAutoStartBreaks, autoStart);

	emit autoStartBreaksChanged();
}

void	AppSettings::setAutoStartFocus(bool autoStart)
{
	if (_autoStartFocus == autoStart)
		return;

	_autoStartFocus = autoStart;
	store(KeyAutoStartFocus, autoStart);

	emit autoStartFocusChanged();
}

void	AppSettings::setAlarmVolume(qreal volume)
{
	volume = qBound(0.0, volume, 1.0);

	if (qFuzzyCompare(_alarmVolume, volume))
		return;

	_alarmVolume = volume;
	store(KeyAlarmVolume, volume);

	emit alarmVolumeChanged();
}

void	AppSettings::setStreamUrl(const QString &url)
{
	QString	trimmed = url.trimmed();

	if (_streamUrl == trimmed)
		return;

	_streamUrl = trimmed;
	store(KeyStreamUrl, trimmed);

	emit streamUrlChanged();
}

void	AppSettings::setMusicVolume(qreal volume)
{
	volume = qBound(0.0, volume, 1.0);

	if (qFuzzyCompare(_musicVolume, volume))
		return;

	_musicVolume = volume;
	store(KeyMusicVolume, volume);

	emit musicVolumeChanged();
}

void	AppSettings::setMusicFollowsFocus(bool follows)
{
	if (_musicFollowsFocus == follows)
		return;

	_musicFollowsFocus = follows;
	store(KeyMusicFollowsFocus, follows);

	emit musicFollowsFocusChanged();
}

void	AppSettings::setAlwaysOnTop(bool onTop)
{
	if (_alwaysOnTop == onTop)
		return;

	_alwaysOnTop = onTop;
	store(KeyAlwaysOnTop, onTop);

	emit alwaysOnTopChanged();
}

void	AppSettings::setCloseMinimizes(bool minimizes)
{
	if (_closeMinimizes == minimizes)
		return;

	_closeMinimizes = minimizes;
	store(KeyCloseMinimizes, minimizes);

	emit closeMinimizesChanged();
}

void	AppSettings::setMiniTimer(bool value)
{
	if (_miniTimer == value)
		return;

	_miniTimer = value;
	store(KeyMiniTimer, value);

	emit miniTimerChanged();
}

void	AppSettings::setMiniTimerX(int value)
{
	if (_miniTimerX == value)
		return;

	_miniTimerX = value;
	store(KeyMiniTimerX, value);

	emit miniTimerXChanged();
}

void	AppSettings::setMiniTimerY(int value)
{
	if (_miniTimerY == value)
		return;

	_miniTimerY = value;
	store(KeyMiniTimerY, value);

	emit miniTimerYChanged();
}

void	AppSettings::setTasksEnabled(bool value)
{
	if (_tasksEnabled == value)
		return;

	_tasksEnabled = value;
	store(KeyTasksEnabled, value);

	emit tasksEnabledChanged();
}

void	AppSettings::setMusicSource(const QString &value)
{
	if (_musicSource == value || !musicSources().contains(value))
		return;

	_musicSource = value;
	store(KeyMusicSource, value);

	emit musicSourceChanged();
}

void	AppSettings::setRadioUrl(const QString &url)
{
	QString	value = url.trimmed();

	if (_radioUrl == value)
		return;

	_radioUrl = value;
	store(KeyRadioUrl, value);

	emit radioUrlChanged();
}

void	AppSettings::setYoutubeUrl(const QString &url)
{
	QString	value = url.trimmed();

	if (_youtubeUrl == value)
		return;

	_youtubeUrl = value;
	store(KeyYoutubeUrl, value);

	emit youtubeUrlChanged();
}

void	AppSettings::setSpotifyUri(const QString &url)
{
	QString	value = url.trimmed();

	if (_spotifyUri == value)
		return;

	_spotifyUri = value;
	store(KeySpotifyUri, value);

	emit spotifyUriChanged();
}

void	AppSettings::setSpotifyClientId(const QString &url)
{
	QString	value = url.trimmed();

	if (_spotifyClientId == value)
		return;

	_spotifyClientId = value;
	store(KeySpotifyClientId, value);

	emit spotifyClientIdChanged();
}

void	AppSettings::setCustomStations(const QString &value)
{
	if (_customStations == value)
		return;

	_customStations = value;
	store(KeyCustomStations, value);

	emit customStationsChanged();
}

void	AppSettings::setSpotifySaved(const QString &value)
{
	if (_spotifySaved == value)
		return;

	_spotifySaved = value;
	store(KeySpotifySaved, value);

	emit spotifySavedChanged();
}

void	AppSettings::setFocusColor(const QString &value)
{
	if (_focusColor == value)
		return;

	_focusColor = value;
	store(KeyFocusColor, value);

	emit focusColorChanged();
}

void	AppSettings::setShortBreakColor(const QString &value)
{
	if (_shortBreakColor == value)
		return;

	_shortBreakColor = value;
	store(KeyShortBreakColor, value);

	emit shortBreakColorChanged();
}

void	AppSettings::setLongBreakColor(const QString &value)
{
	if (_longBreakColor == value)
		return;

	_longBreakColor = value;
	store(KeyLongBreakColor, value);

	emit longBreakColorChanged();
}

void	AppSettings::setTimerStyle(const QString &value)
{
	if (_timerStyle == value)
		return;

	_timerStyle = value;
	store(KeyTimerStyle, value);

	emit timerStyleChanged();
}

void	AppSettings::setAppFont(const QString &value)
{
	if (_appFont == value)
		return;

	_appFont = value;
	store(KeyAppFont, value);

	emit appFontChanged();
}

void	AppSettings::setFontOnTitle(bool value)
{
	if (_fontOnTitle == value)
		return;

	_fontOnTitle = value;
	store(KeyFontOnTitle, value);

	emit fontOnTitleChanged();
}

void	AppSettings::restoreDefaults()
{
	setFocusColor(QString());
	setShortBreakColor(QString());
	setLongBreakColor(QString());
	setTimerStyle(QString());
	setAppFont(QString());
	setFontOnTitle(DefaultFontOnTitle);

	setFocusMinutes(DefaultFocusMinutes);
	setShortBreakMinutes(DefaultShortBreakMinutes);
	setLongBreakMinutes(DefaultLongBreakMinutes);
	setRoundsBeforeLongBreak(DefaultRoundsBeforeLongBreak);
	setAutoStartBreaks(DefaultAutoStartBreaks);
	setAutoStartFocus(DefaultAutoStartFocus);
	setAlarmVolume(DefaultAlarmVolume);
	setStreamUrl(defaultStreamUrl());
	setMusicVolume(DefaultMusicVolume);
	setMusicFollowsFocus(DefaultMusicFollowsFocus);
	setAlwaysOnTop(DefaultAlwaysOnTop);
	setCloseMinimizes(DefaultCloseMinimizes);
	setMiniTimer(DefaultMiniTimer);
	setTasksEnabled(DefaultTasksEnabled);
	setMusicSource(QStringLiteral("radio"));
	setRadioUrl(defaultStreamUrl());
	setYoutubeUrl(defaultYoutubeUrl());
}

void	AppSettings::load()
{
	// Values are clamped on the way in as well as on the way out, so a hand edited
	// or corrupted config file cannot put the timer into a nonsense state.
	_focusMinutes = qBound(MinimumMinutes,
		_store.value(KeyFocusMinutes, DefaultFocusMinutes).toInt(), MaximumMinutes);

	_shortBreakMinutes = qBound(MinimumMinutes,
		_store.value(KeyShortBreakMinutes, DefaultShortBreakMinutes).toInt(), MaximumMinutes);

	_longBreakMinutes = qBound(MinimumMinutes,
		_store.value(KeyLongBreakMinutes, DefaultLongBreakMinutes).toInt(), MaximumMinutes);

	_roundsBeforeLongBreak = qBound(MinimumRounds,
		_store.value(KeyRoundsBeforeLongBreak, DefaultRoundsBeforeLongBreak).toInt(), MaximumRounds);

	_autoStartBreaks = _store.value(KeyAutoStartBreaks, DefaultAutoStartBreaks).toBool();
	_autoStartFocus = _store.value(KeyAutoStartFocus, DefaultAutoStartFocus).toBool();

	_alarmVolume = qBound(0.0, _store.value(KeyAlarmVolume, DefaultAlarmVolume).toDouble(), 1.0);

	_streamUrl = _store.value(KeyStreamUrl, defaultStreamUrl()).toString().trimmed();
	_musicVolume = qBound(0.0, _store.value(KeyMusicVolume, DefaultMusicVolume).toDouble(), 1.0);
	_musicFollowsFocus = _store.value(KeyMusicFollowsFocus, DefaultMusicFollowsFocus).toBool();

	_alwaysOnTop = _store.value(KeyAlwaysOnTop, DefaultAlwaysOnTop).toBool();
	_closeMinimizes = _store.value(KeyCloseMinimizes, DefaultCloseMinimizes).toBool();
	_miniTimer = _store.value(KeyMiniTimer, DefaultMiniTimer).toBool();
	_miniTimerX = _store.value(KeyMiniTimerX, DefaultMiniTimerX).toInt();
	_miniTimerY = _store.value(KeyMiniTimerY, DefaultMiniTimerY).toInt();
	_tasksEnabled = _store.value(KeyTasksEnabled, DefaultTasksEnabled).toBool();
	// Before there was a choice of source there was only the stream URL. Someone who had
	// changed it keeps their link as a custom one; everyone else starts on the radio list,
	// on the same station they were already hearing.
	QString	legacySource = _streamUrl == defaultStreamUrl() || _streamUrl.isEmpty()
		? QStringLiteral("radio")
		: QStringLiteral("custom");

	_musicSource = _store.value(KeyMusicSource, legacySource).toString();

	_radioUrl = _store.value(KeyRadioUrl, defaultStreamUrl()).toString().trimmed();
	_youtubeUrl = _store.value(KeyYoutubeUrl, defaultYoutubeUrl()).toString().trimmed();
	_spotifyUri = _store.value(KeySpotifyUri, QStringLiteral("")).toString();
	_spotifyClientId = _store.value(KeySpotifyClientId, QStringLiteral("")).toString();
	_customStations = _store.value(KeyCustomStations, QStringLiteral("")).toString();

	// "custom" used to be a source of its own, a single link beside the radio list. Links
	// are stations now: whoever was on one finds it saved at the end of the list and
	// playing from there, instead of losing it.
	if (_musicSource == QLatin1String("custom"))
	{
		QJsonArray	stations = QJsonDocument::fromJson(_customStations.toUtf8()).array();
		bool		known = false;

		for (const QJsonValue &station : std::as_const(stations))
			known = known || station.toObject().value(QStringLiteral("url")).toString() == _streamUrl;

		if (!_streamUrl.isEmpty() && !known)
		{
			stations.append(QJsonObject {
				{ QStringLiteral("name"), QStringLiteral("My stream") },
				{ QStringLiteral("note"), QStringLiteral("Your own link") },
				{ QStringLiteral("url"), _streamUrl }
			});

			_customStations = QString::fromUtf8(QJsonDocument(stations).toJson(QJsonDocument::Compact));
			store(KeyCustomStations, _customStations);
		}

		if (!_streamUrl.isEmpty())
		{
			_radioUrl = _streamUrl;
			store(KeyRadioUrl, _radioUrl);
		}
	}

	if (!musicSources().contains(_musicSource))
	{
		_musicSource = QStringLiteral("radio");
		store(KeyMusicSource, _musicSource);
	}
	_spotifySaved = _store.value(KeySpotifySaved, QStringLiteral("")).toString();
	_focusColor = _store.value(KeyFocusColor, QStringLiteral("")).toString();
	_shortBreakColor = _store.value(KeyShortBreakColor, QStringLiteral("")).toString();
	_longBreakColor = _store.value(KeyLongBreakColor, QStringLiteral("")).toString();
	_timerStyle = _store.value(KeyTimerStyle, QStringLiteral("")).toString();
	_appFont = _store.value(KeyAppFont, QStringLiteral("")).toString();
	_fontOnTitle = _store.value(KeyFontOnTitle, DefaultFontOnTitle).toBool();
}

void	AppSettings::store(const char *key, const QVariant &value)
{
	_store.setValue(QLatin1String(key), value);
}
