#include "ShellIdentity.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>

// Makes the app show up in the desktop's launcher, searchable by name and with its icon,
// without asking for root.
//
// An AppImage is a single file: nothing installs it, so nothing tells GNOME or KDE that
// it exists. The freedesktop answer is a .desktop entry under ~/.local/share, which is
// per-user and needs no privileges, so the app writes its own the first time it runs.
// The same code covers a plain build being run from a directory.
//
// Deliberately conservative: the entry is rewritten only when it is missing or points at
// a different binary, so moving or updating the AppImage fixes itself while a user who
// deleted the entry on purpose does not get it back on every launch.

namespace
{
	const char *const	EntryName = "pomodoro.desktop";
	const char *const	IconName = "pomodoro";

	const char *const	IconResource = ":/qt/qml/Pomodoro/assets/icons/pomodoroLogoTransport.png";

	// Where the binary should be launched from. An AppImage is mounted at a temporary
	// path while it runs, so applicationFilePath() would point inside that mount and be
	// gone by the next boot; APPIMAGE holds the real file's location.
	QString	launcherPath()
	{
		QByteArray	appImage = qgetenv("APPIMAGE");

		if (!appImage.isEmpty())
			return QString::fromLocal8Bit(appImage);

		return QCoreApplication::applicationFilePath();
	}

	// The icon has to be a real file on disk for the icon theme to find it, and the
	// hicolor theme is the one every desktop falls back to.
	bool	installIcon()
	{
		QString	directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
			+ QStringLiteral("/icons/hicolor/256x256/apps");

		if (!QDir().mkpath(directory))
			return false;

		QString	path = directory + QLatin1Char('/') + QLatin1String(IconName) + QStringLiteral(".png");

		if (QFile::exists(path))
			return true;

		QImage	image(QString::fromLatin1(IconResource));

		if (image.isNull())
			return false;

		// 256 is the largest size the hicolor directory above is declared for; the source
		// logo is 500x500.
		return image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(path, "PNG");
	}

	QString	readExecLine(const QString &path)
	{
		QFile	file(path);

		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return QString();

		while (!file.atEnd())
		{
			QString	line = QString::fromUtf8(file.readLine()).trimmed();

			if (line.startsWith(QLatin1String("Exec=")))
				return line.mid(5).trimmed();
		}

		return QString();
	}

	bool	writeEntry(const QString &path, const QString &target)
	{
		QSaveFile	file(path);

		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return false;

		// Quoted so a path containing spaces still launches, and the trailing fields match
		// what packaging/pomodoro.desktop ships for a system-wide install.
		QString	entry = QStringLiteral(
			"[Desktop Entry]\n"
			"Type=Application\n"
			"Version=1.1\n"
			"Name=Pomodoro\n"
			"GenericName=Focus Timer\n"
			"Comment=A focus timer with session statistics and a lo-fi stream\n"
			"Exec=\"%1\"\n"
			"Icon=%2\n"
			"Terminal=false\n"
			"Categories=Utility;\n"
			"Keywords=pomodoro;timer;focus;productivity;\n"
			"StartupNotify=true\n"
			"StartupWMClass=pomodoro\n").arg(target, QLatin1String(IconName));

		file.write(entry.toUtf8());

		return file.commit();
	}
}

void	registerShellIdentity()
{
	QString	applications = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
		+ QStringLiteral("/applications");

	if (!QDir().mkpath(applications))
		return;

	QString	path = applications + QLatin1Char('/') + QLatin1String(EntryName);
	QString	target = launcherPath();

	// Already describes this copy, so leave the user's launcher alone.
	if (QFile::exists(path))
	{
		QString	current = readExecLine(path);

		// The Exec line is written quoted, so compare against both spellings.
		if (current == target || current == QLatin1Char('"') + target + QLatin1Char('"'))
			return;
	}

	installIcon();

	if (!writeEntry(path, target))
	{
		qWarning("pomodoro: could not write %s, the app will not appear in the launcher",
			qPrintable(path));
	}
}
