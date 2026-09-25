#include "MediaControls.hpp"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

// MPRIS 2: the org.mpris.MediaPlayer2 service that GNOME's media widget, KDE's media
// controller, playerctl and the keyboard's play key all speak. Two adaptors on one
// object path, and a PropertiesChanged signal sent by hand, because QDBus exports
// properties but never announces when they change.
//
// The bus name is only claimed while the app has music to offer, so the timer does
// not sit in the media widget as a player with nothing playing.
//
// A named namespace rather than an anonymous one: moc has to be able to spell the
// adaptors' fully qualified names in the code it generates.

namespace Mpris
{
	const char *const	ServiceName = "org.mpris.MediaPlayer2.pomodoro";
	const char *const	ObjectPath = "/org/mpris/MediaPlayer2";
	const char *const	PlayerInterface = "org.mpris.MediaPlayer2.Player";

	// Mandatory even for a stream. Any valid object path will do as long as it is not
	// /org/mpris/MediaPlayer2/TrackList/NoTrack, which means "nothing".
	const char *const	TrackId = "/org/elhiba/pomodoro/track/stream";

	class MprisMediaControls;

	class RootAdaptor : public QDBusAbstractAdaptor
	{
		Q_OBJECT
		Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")

		Q_PROPERTY(bool CanQuit READ canQuit)
		Q_PROPERTY(bool CanRaise READ canRaise)
		Q_PROPERTY(bool HasTrackList READ hasTrackList)
		Q_PROPERTY(QString Identity READ identity)
		Q_PROPERTY(QString DesktopEntry READ desktopEntry)
		Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
		Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

		public:
			explicit RootAdaptor(QObject *parent)
				: QDBusAbstractAdaptor(parent)
			{
			}

			bool	canQuit() const { return false; }
			bool	canRaise() const { return false; }
			bool	hasTrackList() const { return false; }

			QString	identity() const { return QStringLiteral("Pomodoro"); }
			QString	desktopEntry() const { return QCoreApplication::applicationName(); }

			QStringList	supportedUriSchemes() const { return QStringList(); }
			QStringList	supportedMimeTypes() const { return QStringList(); }

		public slots:
			void	Raise() {}
			void	Quit() {}
	};

	class PlayerAdaptor : public QDBusAbstractAdaptor
	{
		Q_OBJECT
		Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

		Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
		Q_PROPERTY(double Rate READ rate WRITE setRate)
		Q_PROPERTY(QVariantMap Metadata READ metadata)
		Q_PROPERTY(double Volume READ volume WRITE setVolume)
		Q_PROPERTY(qlonglong Position READ position)
		Q_PROPERTY(double MinimumRate READ minimumRate)
		Q_PROPERTY(double MaximumRate READ maximumRate)
		Q_PROPERTY(bool CanGoNext READ canGoNext)
		Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
		Q_PROPERTY(bool CanPlay READ canPlay)
		Q_PROPERTY(bool CanPause READ canPause)
		Q_PROPERTY(bool CanSeek READ canSeek)
		Q_PROPERTY(bool CanControl READ canControl)

		public:
			explicit PlayerAdaptor(MprisMediaControls *controls);

			QString		playbackStatus() const;
			QVariantMap	metadata() const;

			double	rate() const { return 1.0; }
			void	setRate(double) {}
			double	volume() const { return 1.0; }
			void	setVolume(double) {}
			qlonglong	position() const { return 0; }
			double	minimumRate() const { return 1.0; }
			double	maximumRate() const { return 1.0; }

			bool	canGoNext() const { return true; }
			bool	canGoPrevious() const { return true; }
			bool	canPlay() const { return true; }
			bool	canPause() const { return true; }
			bool	canSeek() const { return false; }
			bool	canControl() const { return true; }

		public slots:
			void	Next();
			void	Previous();
			void	Pause();
			void	PlayPause();
			void	Stop();
			void	Play();
			void	Seek(qlonglong) {}
			void	SetPosition(const QDBusObjectPath &, qlonglong) {}
			void	OpenUri(const QString &) {}

		signals:
			void	Seeked(qlonglong position);

		private:
			MprisMediaControls	*_controls;
	};

	class MprisMediaControls final : public MediaControls
	{
		public:
			explicit MprisMediaControls(QObject *parent)
				: MediaControls(parent)
			{
				new RootAdaptor(this);
				new PlayerAdaptor(this);

				QDBusConnection	bus = QDBusConnection::sessionBus();

				// The object stays registered for the life of the app; only the name
				// comes and goes with setEnabled.
				_available = bus.isConnected()
					&& bus.registerObject(QLatin1String(ObjectPath), this,
						QDBusConnection::ExportAdaptors);

				QString	artwork = artworkPath();

				if (!artwork.isEmpty())
					_artUrl = QUrl::fromLocalFile(artwork).toString();
			}

			~MprisMediaControls() override
			{
				if (_registered)
					QDBusConnection::sessionBus().unregisterService(QLatin1String(ServiceName));
			}

			bool	available() const
			{
				return _available;
			}

			void	setEnabled(bool enabled) override
			{
				if (!_available || _registered == enabled)
					return;

				QDBusConnection	bus = QDBusConnection::sessionBus();

				if (enabled)
					_registered = bus.registerService(QLatin1String(ServiceName));
				else
				{
					bus.unregisterService(QLatin1String(ServiceName));
					_registered = false;
				}
			}

			void	setPlaybackState(PlaybackState state) override
			{
				if (_state == state)
					return;

				_state = state;

				QVariantMap	changed;

				changed.insert(QStringLiteral("PlaybackStatus"), playbackStatus());
				announce(changed);
			}

			void	setNowPlaying(const QString &title, const QString &artist) override
			{
				if (_title == title && _artist == artist)
					return;

				_title = title;
				_artist = artist;

				QVariantMap	changed;

				changed.insert(QStringLiteral("Metadata"), metadata());
				announce(changed);
			}

			void	setArtwork(const QString &path) override
			{
				QString	chosen = path.isEmpty() ? artworkPath() : path;
				QString	url = chosen.isEmpty() ? QString() : QUrl::fromLocalFile(chosen).toString();

				if (url == _artUrl)
					return;

				_artUrl = url;

				QVariantMap	changed;

				changed.insert(QStringLiteral("Metadata"), metadata());
				announce(changed);
			}

			QString	playbackStatus() const
			{
				switch (_state)
				{
					case Playing:
						return QStringLiteral("Playing");
					case Paused:
						return QStringLiteral("Paused");
					case Stopped:
					default:
						return QStringLiteral("Stopped");
				}
			}

			QVariantMap	metadata() const
			{
				QVariantMap	map;

				map.insert(QStringLiteral("mpris:trackid"),
					QVariant::fromValue(QDBusObjectPath(QLatin1String(TrackId))));

				if (!_title.isEmpty())
					map.insert(QStringLiteral("xesam:title"), _title);

				if (!_artist.isEmpty())
					map.insert(QStringLiteral("xesam:artist"), QStringList(_artist));

				if (!_artUrl.isEmpty())
					map.insert(QStringLiteral("mpris:artUrl"), _artUrl);

				return map;
			}

		private:
			bool	_available = false;
			bool	_registered = false;

			PlaybackState	_state = Stopped;
			QString			_title;
			QString			_artist;
			QString			_artUrl;

			// org.freedesktop.DBus.Properties.PropertiesChanged, which is how every
			// MPRIS client learns that something moved.
			void	announce(const QVariantMap &changed)
			{
				if (!_registered)
					return;

				QDBusMessage	signal = QDBusMessage::createSignal(
					QLatin1String(ObjectPath),
					QStringLiteral("org.freedesktop.DBus.Properties"),
					QStringLiteral("PropertiesChanged"));

				signal << QLatin1String(PlayerInterface) << changed << QStringList();

				QDBusConnection::sessionBus().send(signal);
			}
	};

	PlayerAdaptor::PlayerAdaptor(MprisMediaControls *controls)
		: QDBusAbstractAdaptor(controls)
		, _controls(controls)
	{
	}

	QString	PlayerAdaptor::playbackStatus() const
	{
		return _controls->playbackStatus();
	}

	QVariantMap	PlayerAdaptor::metadata() const
	{
		return _controls->metadata();
	}

	void	PlayerAdaptor::Next()
	{
		emit _controls->nextRequested();
	}

	void	PlayerAdaptor::Previous()
	{
		emit _controls->previousRequested();
	}

	void	PlayerAdaptor::Pause()
	{
		emit _controls->pauseRequested();
	}

	void	PlayerAdaptor::PlayPause()
	{
		emit _controls->toggleRequested();
	}

	void	PlayerAdaptor::Stop()
	{
		emit _controls->stopRequested();
	}

	void	PlayerAdaptor::Play()
	{
		emit _controls->playRequested();
	}
}

MediaControls	*createPlatformMediaControls(QObject *parent)
{
	Mpris::MprisMediaControls	*controls = new Mpris::MprisMediaControls(parent);

	if (controls->available())
		return controls;

	// No session bus: nothing to register with, so fall back to the quiet bridge.
	delete controls;

	return nullptr;
}

#include "MediaControlsMpris.moc"
