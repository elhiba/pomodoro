#ifndef APP_SETTINGS_HPP
#define APP_SETTINGS_HPP

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <QtQml/qqmlregistration.h>

// Everything the app remembers between runs. Registered as a QML singleton, so any
// component can read it without it having to be handed down the tree, and every setter
// writes straight through to QSettings instead of waiting for a save step.
//
// Named AppSettings rather than Settings on purpose: QtCore already exports a QML type
// called Settings, and the two would collide.
class AppSettings : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(int focusMinutes READ focusMinutes WRITE setFocusMinutes NOTIFY focusMinutesChanged)
	Q_PROPERTY(int shortBreakMinutes READ shortBreakMinutes WRITE setShortBreakMinutes NOTIFY shortBreakMinutesChanged)
	Q_PROPERTY(int longBreakMinutes READ longBreakMinutes WRITE setLongBreakMinutes NOTIFY longBreakMinutesChanged)
	Q_PROPERTY(int roundsBeforeLongBreak READ roundsBeforeLongBreak WRITE setRoundsBeforeLongBreak NOTIFY roundsBeforeLongBreakChanged)

	Q_PROPERTY(bool autoStartBreaks READ autoStartBreaks WRITE setAutoStartBreaks NOTIFY autoStartBreaksChanged)
	Q_PROPERTY(bool autoStartFocus READ autoStartFocus WRITE setAutoStartFocus NOTIFY autoStartFocusChanged)

	Q_PROPERTY(qreal alarmVolume READ alarmVolume WRITE setAlarmVolume NOTIFY alarmVolumeChanged)

	Q_PROPERTY(QString streamUrl READ streamUrl WRITE setStreamUrl NOTIFY streamUrlChanged)
	Q_PROPERTY(qreal musicVolume READ musicVolume WRITE setMusicVolume NOTIFY musicVolumeChanged)
	Q_PROPERTY(bool musicFollowsFocus READ musicFollowsFocus WRITE setMusicFollowsFocus NOTIFY musicFollowsFocusChanged)
	Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)
	Q_PROPERTY(bool closeMinimizes READ closeMinimizes WRITE setCloseMinimizes NOTIFY closeMinimizesChanged)

	// The small always-on-top timer shown while the main window is minimised, and where
	// it was last dragged to. -1 means never placed, and QML picks a corner.
	Q_PROPERTY(bool miniTimer READ miniTimer WRITE setMiniTimer NOTIFY miniTimerChanged)
	Q_PROPERTY(int miniTimerX READ miniTimerX WRITE setMiniTimerX NOTIFY miniTimerXChanged)
	Q_PROPERTY(int miniTimerY READ miniTimerY WRITE setMiniTimerY NOTIFY miniTimerYChanged)
	// Whether the task list is offered at all. Off hides its button, drawer and the line
	// under the timer; the tasks themselves are kept.
	Q_PROPERTY(bool tasksEnabled READ tasksEnabled WRITE setTasksEnabled NOTIFY tasksEnabledChanged)
	// Where the music comes from, and each source's own choice, kept separately so that
	// switching between them does not lose any.
	Q_PROPERTY(QString musicSource READ musicSource WRITE setMusicSource NOTIFY musicSourceChanged)
	Q_PROPERTY(QString radioUrl READ radioUrl WRITE setRadioUrl NOTIFY radioUrlChanged)
	Q_PROPERTY(QString youtubeUrl READ youtubeUrl WRITE setYoutubeUrl NOTIFY youtubeUrlChanged)
	Q_PROPERTY(QString spotifyUri READ spotifyUri WRITE setSpotifyUri NOTIFY spotifyUriChanged)
	Q_PROPERTY(QString spotifyClientId READ spotifyClientId WRITE setSpotifyClientId NOTIFY spotifyClientIdChanged)
	// The stations the user added to the radio list, as a JSON array of
	// { name, note, url }. JSON rather than a QVariantList so the registry holds
	// readable text instead of a serialised QVariant blob.
	Q_PROPERTY(QString customStations READ customStations WRITE setCustomStations NOTIFY customStationsChanged)
	// Spotify links the user saved to the music panel's shelf, as a JSON array of
	// { name, uri, image }, stored like customStations.
	Q_PROPERTY(QString spotifySaved READ spotifySaved WRITE setSpotifySaved NOTIFY spotifySavedChanged)
	// Appearance. Colours are "#rrggbb" per session kind, empty for the built-in one;
	// timerStyle is how the digits change ("" = still, "roll", "flip", "soft"); appFont is
	// the font family for the whole app, timer included, empty for the built-in ones
	// (JetBrains Mono for the timer, the system font elsewhere). fontOnTitle puts it on
	// the "Pomodoro" title too, which otherwise keeps its script font.
	Q_PROPERTY(QString focusColor READ focusColor WRITE setFocusColor NOTIFY focusColorChanged)
	Q_PROPERTY(QString shortBreakColor READ shortBreakColor WRITE setShortBreakColor NOTIFY shortBreakColorChanged)
	Q_PROPERTY(QString longBreakColor READ longBreakColor WRITE setLongBreakColor NOTIFY longBreakColorChanged)
	Q_PROPERTY(QString timerStyle READ timerStyle WRITE setTimerStyle NOTIFY timerStyleChanged)
	Q_PROPERTY(QString appFont READ appFont WRITE setAppFont NOTIFY appFontChanged)
	Q_PROPERTY(bool fontOnTitle READ fontOnTitle WRITE setFontOnTitle NOTIFY fontOnTitleChanged)
	// Whether the Discord activity mentions the song playing (DiscordPresence itself
	// keeps whether the activity is shown at all).
	Q_PROPERTY(bool discordShowMusic READ discordShowMusic WRITE setDiscordShowMusic NOTIFY discordShowMusicChanged)

	// Handy for the settings panel, so the bounds live in one place instead of
	// being repeated in QML.
	Q_PROPERTY(int minimumMinutes READ minimumMinutes CONSTANT)
	Q_PROPERTY(int maximumMinutes READ maximumMinutes CONSTANT)
	Q_PROPERTY(int minimumRounds READ minimumRounds CONSTANT)
	Q_PROPERTY(int maximumRounds READ maximumRounds CONSTANT)

	public:
		static constexpr int	DefaultFocusMinutes = 25;
		static constexpr int	DefaultShortBreakMinutes = 5;
		static constexpr int	DefaultLongBreakMinutes = 15;
		static constexpr int	DefaultRoundsBeforeLongBreak = 4;
		static constexpr bool	DefaultAutoStartBreaks = false;
		static constexpr bool	DefaultAutoStartFocus = false;
		static constexpr qreal	DefaultAlarmVolume = 0.7;
		static constexpr qreal	DefaultMusicVolume = 0.5;
		static constexpr bool	DefaultMusicFollowsFocus = false;
		static constexpr bool	DefaultAlwaysOnTop = false;
		// On by default: the timer is meant to be left running, and pressing close out of
		// habit should not throw away a session.
		static constexpr bool	DefaultCloseMinimizes = true;

		// On: asked for in issue #1, and it only ever appears when the window is minimised.
		static constexpr bool	DefaultMiniTimer = true;
		static constexpr int	DefaultMiniTimerX = -1;
		static constexpr int	DefaultMiniTimerY = -1;
		static constexpr bool	DefaultTasksEnabled = true;
		static constexpr bool	DefaultFontOnTitle = false;
		static constexpr bool	DefaultDiscordShowMusic = true;

		static constexpr int	MinimumMinutes = 1;
		static constexpr int	MaximumMinutes = 120;
		static constexpr int	MinimumRounds = 1;
		static constexpr int	MaximumRounds = 12;

		// The owner's own lo-fi stream, carried over from the settings menu they wrote
		// before the QML rewrite.
		static const QString	&defaultStreamUrl();
		static const QString	&defaultYoutubeUrl();

		// "radio", "youtube", "spotify": which of the music settings below is the one
		// playing. "custom" was a fourth once; load() turns it into a saved station.
		static const QStringList	&musicSources();

		explicit AppSettings(QObject *parent = nullptr);
		~AppSettings() override;

		int		focusMinutes() const;
		int		shortBreakMinutes() const;
		int		longBreakMinutes() const;
		int		roundsBeforeLongBreak() const;
		bool	autoStartBreaks() const;
		bool	autoStartFocus() const;
		qreal	alarmVolume() const;
		QString	streamUrl() const;
		qreal	musicVolume() const;
		bool	musicFollowsFocus() const;
		bool	alwaysOnTop() const;
		bool	closeMinimizes() const;
		bool	miniTimer() const;
		int		miniTimerX() const;
		int		miniTimerY() const;
		bool	tasksEnabled() const;
		QString	musicSource() const;
		QString	radioUrl() const;
		QString	youtubeUrl() const;
		QString	spotifyUri() const;
		QString	spotifyClientId() const;
		QString	customStations() const;
		QString	spotifySaved() const;
		QString	focusColor() const;
		QString	shortBreakColor() const;
		QString	longBreakColor() const;
		QString	timerStyle() const;
		QString	appFont() const;
		bool	fontOnTitle() const;
		bool	discordShowMusic() const;

		int		minimumMinutes() const;
		int		maximumMinutes() const;
		int		minimumRounds() const;
		int		maximumRounds() const;

		void	setFocusMinutes(int minutes);
		void	setShortBreakMinutes(int minutes);
		void	setLongBreakMinutes(int minutes);
		void	setRoundsBeforeLongBreak(int rounds);
		void	setAutoStartBreaks(bool autoStart);
		void	setAutoStartFocus(bool autoStart);
		void	setAlarmVolume(qreal volume);
		void	setStreamUrl(const QString &url);
		void	setMusicVolume(qreal volume);
		void	setMusicFollowsFocus(bool follows);
		void	setAlwaysOnTop(bool onTop);
		void	setCloseMinimizes(bool minimizes);
		void	setMiniTimer(bool value);
		void	setMiniTimerX(int value);
		void	setMiniTimerY(int value);
		void	setTasksEnabled(bool value);
		void	setMusicSource(const QString &value);
		void	setRadioUrl(const QString &url);
		void	setYoutubeUrl(const QString &url);
		void	setSpotifyUri(const QString &url);
		void	setSpotifyClientId(const QString &url);
		void	setCustomStations(const QString &value);
		void	setSpotifySaved(const QString &value);
		void	setFocusColor(const QString &value);
		void	setShortBreakColor(const QString &value);
		void	setLongBreakColor(const QString &value);
		void	setTimerStyle(const QString &value);
		void	setAppFont(const QString &value);
		void	setFontOnTitle(bool value);
		void	setDiscordShowMusic(bool value);

	public slots:
		void	restoreDefaults();

	signals:
		void	focusMinutesChanged();
		void	shortBreakMinutesChanged();
		void	longBreakMinutesChanged();
		void	roundsBeforeLongBreakChanged();
		void	autoStartBreaksChanged();
		void	autoStartFocusChanged();
		void	alarmVolumeChanged();
		void	streamUrlChanged();
		void	musicVolumeChanged();
		void	musicFollowsFocusChanged();
		void	alwaysOnTopChanged();
		void	closeMinimizesChanged();
		void	miniTimerChanged();
		void	miniTimerXChanged();
		void	miniTimerYChanged();
		void	tasksEnabledChanged();
		void	musicSourceChanged();
		void	radioUrlChanged();
		void	youtubeUrlChanged();
		void	spotifyUriChanged();
		void	spotifyClientIdChanged();
		void	customStationsChanged();
		void	spotifySavedChanged();
		void	focusColorChanged();
		void	shortBreakColorChanged();
		void	longBreakColorChanged();
		void	timerStyleChanged();
		void	appFontChanged();
		void	fontOnTitleChanged();
		void	discordShowMusicChanged();

	private:
		QSettings	_store;

		int		_focusMinutes = DefaultFocusMinutes;
		int		_shortBreakMinutes = DefaultShortBreakMinutes;
		int		_longBreakMinutes = DefaultLongBreakMinutes;
		int		_roundsBeforeLongBreak = DefaultRoundsBeforeLongBreak;
		bool	_autoStartBreaks = DefaultAutoStartBreaks;
		bool	_autoStartFocus = DefaultAutoStartFocus;
		qreal	_alarmVolume = DefaultAlarmVolume;

		QString	_streamUrl;
		qreal	_musicVolume = DefaultMusicVolume;
		bool	_musicFollowsFocus = DefaultMusicFollowsFocus;
		bool	_alwaysOnTop = DefaultAlwaysOnTop;
		bool	_closeMinimizes = DefaultCloseMinimizes;
		bool	_miniTimer = DefaultMiniTimer;
		int		_miniTimerX = DefaultMiniTimerX;
		int		_miniTimerY = DefaultMiniTimerY;
		bool	_tasksEnabled = DefaultTasksEnabled;
		QString	_musicSource;
		QString	_radioUrl;
		QString	_youtubeUrl;
		QString	_spotifyUri;
		QString	_spotifyClientId;
		QString	_customStations;
		QString	_spotifySaved;
		QString	_focusColor;
		QString	_shortBreakColor;
		QString	_longBreakColor;
		QString	_timerStyle;
		QString	_appFont;
		bool	_fontOnTitle = DefaultFontOnTitle;
		bool	_discordShowMusic = DefaultDiscordShowMusic;

		void	load();
		void	store(const char *key, const QVariant &value);
};

#endif
