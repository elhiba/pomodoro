#ifndef MEDIA_CONTROLS_HPP
#define MEDIA_CONTROLS_HPP

#include <QObject>
#include <QString>

// The bridge to whatever the desktop uses to show "now playing" and route the media
// keys: System Media Transport Controls on Windows, MPRIS over D-Bus on Linux, the
// Now Playing centre on macOS.
//
// Exactly one implementation is compiled in, chosen in CMakeLists.txt, and create()
// hands it back behind this interface. Nothing else in the app knows which one it got.
// The implementation only reports what the OS asked for; MusicPlayer decides what
// that means, the same way TrayIcon leaves the timer alone.
class MediaControls : public QObject
{
	Q_OBJECT

	public:
		enum PlaybackState
		{
			Stopped,
			Playing,
			Paused
		};

		// Never returns null: a desktop with nothing to talk to gets a bridge that
		// listens politely and does nothing.
		static MediaControls	*create(QObject *parent = nullptr);

		// Whether the desktop should list this app as a media source at all. Off means
		// the entry disappears from the volume flyout, the media widget and so on, which
		// is right for a timer that only sometimes plays music.
		virtual void	setEnabled(bool enabled) = 0;

		virtual void	setPlaybackState(PlaybackState state) = 0;

		// What the desktop shows. The artist slot carries the station name here, since
		// a stream has no artist of its own to speak of.
		virtual void	setNowPlaying(const QString &title, const QString &artist) = 0;

	signals:
		void	playRequested();
		void	pauseRequested();
		void	toggleRequested();
		void	stopRequested();

	protected:
		explicit MediaControls(QObject *parent = nullptr);

		// Where the artwork the desktop may want comes from. Empty when the resource
		// could not be unpacked, in which case the implementation sends none.
		static QString	artworkPath();
};

// Defined by whichever MediaControls*.cpp is compiled in. Returns null when the
// platform service is not reachable, and create() substitutes the no-op bridge.
MediaControls	*createPlatformMediaControls(QObject *parent);

#endif
