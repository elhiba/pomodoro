#include "ShellIdentity.hpp"
#include "IconFile.hpp"

#include <QBuffer>
#include <QCryptographicHash>

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
// a different binary or logo, so the launcher follows the bundled image on upgrades.

namespace
{
	const char *const	EntryName = "pomodoro.desktop";

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

	// Use an absolute path in the launcher so the shell need not discover a new
	// name through a cached icon-theme index. Keep a standard size on disk.
	QString	installIcon()
	{
		QString	directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
			+ QStringLiteral("/icons/hicolor/256x256/apps");

		if (!QDir().mkpath(directory))
			return QString();

		QImage	image(QString::fromLatin1(IconResource));

		if (image.isNull())
			return QString();

		// Match the size declared by the hicolor directory, regardless of source size.
		QByteArray data;
		QBuffer buffer(&data);
		if (!buffer.open(QIODevice::WriteOnly)
			|| !image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG"))
			return QString();
		// A new name avoids reusing the same desktop icon cache key.
		// Replacing pixels at the same name is not enough for a running GNOME session.
		QString name = QStringLiteral("pomodoro-")
			+ QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex().left(16));
		QString path = directory + QLatin1Char('/') + name + QStringLiteral(".png");
		return IconFile::writeIfChanged(path, data) ? path : QString();
	}

	QString	readEntryValue(const QString &path, const QString &key)
	{
		QFile	file(path);

		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return QString();

		while (!file.atEnd())
		{
			QString	line = QString::fromUtf8(file.readLine()).trimmed();

			if (line.startsWith(key + QLatin1Char('=')))
				return line.mid(key.size() + 1).trimmed();
		}

		return QString();
	}

	bool	writeEntry(const QString &path, const QString &target, const QString &iconPath)
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
			"StartupWMClass=pomodoro\n").arg(target, iconPath);

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

	// An upgrade can keep the same executable path while changing the bundled logo.
	// Refresh the icon before deciding whether the launcher needs rewriting.
	QString iconPath = installIcon();
	if (iconPath.isEmpty())
	{
		qWarning("pomodoro: could not refresh the launcher icon");
		return;
	}

	// Already describes this copy, so leave the user's launcher alone.
	if (QFile::exists(path))
	{
		QString	current = readEntryValue(path, QStringLiteral("Exec"));

		// The Exec line is written quoted, so compare against both spellings.
		if ((current == target || current == QLatin1Char('"') + target + QLatin1Char('"'))
			&& readEntryValue(path, QStringLiteral("Icon")) == iconPath)
			return;
	}

	if (!writeEntry(path, target, iconPath))
	{
		qWarning("pomodoro: could not write %s, the app will not appear in the launcher",
			qPrintable(path));
	}
}
