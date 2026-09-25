#include "MediaControls.hpp"

#include <QDir>
#include <QFile>
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
// a qrc path. The same trick Notifier uses: unpack the logo into the cache once.
void	MediaControls::setArtwork(const QString &)
{
}

QString	MediaControls::artworkPath()
{
	QString	directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

	if (directory.isEmpty() || !QDir().mkpath(directory))
		return QString();

	QString	path = directory + QStringLiteral("/pomodoro-artwork.png");

	if (QFile::exists(path))
		return path;

	QFile	source(QString::fromLatin1(IconResource));

	if (!source.copy(path))
		return QString();

	// Copies of resources come out read only, which would stop a later version of the
	// logo from replacing this one.
	QFile::setPermissions(path,
		QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

	return path;
}
