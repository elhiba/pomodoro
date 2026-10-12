#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QDBusConnection>

#include "IconFile.hpp"
#include "MediaControls.hpp"
#include "Notifier.hpp"
#ifdef Q_OS_LINUX
#include "ShellIdentity.hpp"
#endif

MediaControls *createPlatformMediaControls(QObject *) { return nullptr; }
class ArtworkProbe : public MediaControls
{
	public: using MediaControls::artworkPath;
};

class TestIconCache : public QObject
{
	Q_OBJECT
	QTemporaryDir home;
	const QString resource = QStringLiteral(":/qt/qml/Pomodoro/assets/icons/pomodoroLogoTransport.png");

	static QByteArray read(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return {};
		return file.readAll();
	}

	static bool stamp(const QString &path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadWrite)
			&& file.setFileTime(QDateTime::fromSecsSinceEpoch(1000000000), QFileDevice::FileModificationTime);
	}

private slots:
	void initTestCase()
	{
		QVERIFY(home.isValid());
		QCoreApplication::setOrganizationName("elhiba-tests");
		QCoreApplication::setApplicationName("IconCacheTest");
#ifdef Q_OS_UNIX
		qputenv("XDG_DATA_HOME", (home.path() + "/data").toUtf8());
		qputenv("XDG_CACHE_HOME", (home.path() + "/cache").toUtf8());
		qputenv("XDG_CONFIG_HOME", (home.path() + "/config").toUtf8());
#else
		QStandardPaths::setTestModeEnabled(true);
#endif
		QVERIFY(!QImage(resource).isNull());
	}

	void refreshesOldAndCorruptFiles()
	{
		QString path = home.path() + "/icon.png";
		QVERIFY(IconFile::writeIfChanged(path, "old icon"));
		QVERIFY(IconFile::refreshResource(path, resource));
		QCOMPARE(read(path), read(resource));
		QVERIFY(IconFile::writeIfChanged(path, "corrupt PNG"));
		QVERIFY(IconFile::refreshResource(path, resource));
		QCOMPARE(read(path), read(resource));
	}

	void identicalIconIsNotRewritten()
	{
		QString path = home.path() + "/unchanged.png";
		QVERIFY(IconFile::refreshResource(path, resource));
		QVERIFY(stamp(path));
		auto before = QFileInfo(path).lastModified();
		QVERIFY(IconFile::refreshResource(path, resource));
		QCOMPARE(QFileInfo(path).lastModified(), before);
	}

	void failedRefreshPreservesExistingFile()
	{
		QString path = home.path() + "/preserved.png";
		QVERIFY(IconFile::writeIfChanged(path, "previous image"));
		QVERIFY(!IconFile::refreshResource(path, ":/missing.png"));
		QVERIFY(!IconFile::writeIfChanged(path, {}));
		QCOMPARE(read(path), QByteArray("previous image"));
		QString directory = home.path() + "/blocked.png";
		QVERIFY(QDir().mkpath(directory));
		QVERIFY(!IconFile::refreshResource(directory, resource));
		QVERIFY(QFileInfo(directory).isDir());
	}

	void mediaArtworkRefreshesOnUpgrade()
	{
		QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
		QVERIFY(QDir().mkpath(cache));
		QString path = cache + "/pomodoro-artwork.png";
		QVERIFY(IconFile::writeIfChanged(path, "previous artwork"));
		QCOMPARE(ArtworkProbe::artworkPath(), path);
		QCOMPARE(read(path), read(resource));
		QVERIFY(stamp(path));
		auto before = QFileInfo(path).lastModified();
		QCOMPARE(ArtworkProbe::artworkPath(), path);
		QCOMPARE(QFileInfo(path).lastModified(), before);
	}

	void notificationIconRefreshesOnUpgrade()
	{
		auto bus = QDBusConnection::sessionBus();
		if (!bus.isConnected() || !bus.registerService("org.freedesktop.Notifications"))
			QSKIP("Run under dbus-run-session to test the notification cache in isolation.");
		QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
		QVERIFY(QDir().mkpath(cache));
		QString path = cache + "/pomodoro-notification.png";
		QVERIFY(IconFile::writeIfChanged(path, "previous notification"));
		Notifier notifier;
		bus.unregisterService("org.freedesktop.Notifications");
		QVERIFY(notifier.available());
		QCOMPARE(read(path), read(resource));
	}

#ifdef Q_OS_LINUX
	void launcherRefreshesWithUnchangedExecutable()
	{
		QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
		QString icons = data + "/icons/hicolor/256x256/apps/";
		QString entry = data + "/applications/pomodoro.desktop";
		QVERIFY(QDir().mkpath(icons));
		QVERIFY(QDir().mkpath(QFileInfo(entry).absolutePath()));
		QVERIFY(IconFile::writeIfChanged(icons + "pomodoro.png", "previous dock icon"));
		QByteArray oldEntry = "[Desktop Entry]\nType=Application\nName=Pomodoro\nExec=\""
			+ QCoreApplication::applicationFilePath().toUtf8() + "\"\nIcon=pomodoro\n";
		QVERIFY(IconFile::writeIfChanged(entry, oldEntry));
		registerShellIdentity();
		QByteArray launcher = read(entry);
		QString icon;
		for (const auto &line : launcher.split('\n'))
			if (line.startsWith("Icon="))
				icon = QString::fromUtf8(line.mid(5));
		QVERIFY(QFileInfo(icon).isAbsolute());
		QVERIFY(QFileInfo(icon).fileName().startsWith("pomodoro-"));
		QCOMPARE(QImage(icon), QImage(resource).scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		QVERIFY(stamp(entry));
		auto launcherTime = QFileInfo(entry).lastModified();
		QVERIFY(IconFile::writeIfChanged(icon, "corrupt cached icon"));
		registerShellIdentity();
		QCOMPARE(QImage(icon), QImage(resource).scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		QCOMPARE(read(entry), launcher);
		QCOMPARE(QFileInfo(entry).lastModified(), launcherTime);
		QVERIFY(stamp(icon));
		auto iconTime = QFileInfo(icon).lastModified();
		registerShellIdentity();
		QCOMPARE(QFileInfo(icon).lastModified(), iconTime);
	}

#endif
};

QTEST_GUILESS_MAIN(TestIconCache)
#include "tst_iconcache.moc"
