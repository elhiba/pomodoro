#include "MediaControls.hpp"
#include "IconFile.hpp"

#include <QDir>
#include <QStandardPaths>

namespace
{
	const char *const	IconResource = ":/qt/qml/Pomodoro/assets/icons/pomodoroLogoTransport.png";

	// The bridge for desktops with nothing to bridge to. Keeps every caller free of
	// null checks.
	class NullMediaControls : public MediaControls
	{
		public:
			explicit NullMediaControls(QObject *parent = nullptr)
				: MediaControls(parent)
			{
			}

			void	setEnabled(bool) override {}
			void	setPlaybackState(PlaybackState) override {}
			void	setNowPlaying(const QString &, const QString &) override {}
	};
}

MediaControls::MediaControls(QObject *parent)
	: QObject(parent)
{
}

MediaControls	*MediaControls::create(QObject *parent)
{
	MediaControls	*controls = createPlatformMediaControls(parent);

	if (controls)
		return controls;

	return new NullMediaControls(parent);
}

// MPRIS wants a file URL and macOS wants an image it can load, neither of which can be
// a qrc path. Refresh the cached logo when the bundled resource changes.
void	MediaControls::setArtwork(const QString &)
{
}

QString	MediaControls::artworkPath()
{
	QString	directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

	if (directory.isEmpty() || !QDir().mkpath(directory))
		return QString();

	QString	path = directory + QStringLiteral("/pomodoro-artwork.png");

	if (!IconFile::refreshResource(path, QString::fromLatin1(IconResource)))
		return QString();

	return path;
}
