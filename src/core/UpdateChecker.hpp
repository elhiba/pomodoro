#ifndef UPDATE_CHECKER_HPP
#define UPDATE_CHECKER_HPP

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <QtQml/qqmlregistration.h>

class QNetworkReply;

// Asks GitHub what the newest release is, and when it is newer than the running copy,
// fetches it and puts it in place on its own -- nobody has to click anything, and
// nothing is ever handed off to a browser.
//
// Main.qml runs check() shortly after every launch and every few hours after that. A
// newer release is downloaded in the background straight away; once it is verified it
// waits (ReadyToInstall) until the timer is idle, so a running or paused session is
// never cut short, and Main.qml then calls applyUpdate(). Quitting with an update
// waiting installs it on the way out. Installing depends on how this copy got onto the
// machine:
//
//   * Windows, installed by the Inno Setup installer: the new installer is run silently
//     over the top, and relaunches the app when it is done;
//   * Windows, the portable zip: the new zip is unpacked over this folder by a small
//     PowerShell script once this process has exited, and the app is started again;
//   * Linux AppImage: the new AppImage replaces the file at $APPIMAGE and is started;
//   * macOS: the new dmg is mounted, the bundle this copy runs from is swapped for the
//     one inside it once this process has exited, and the app is started again;
//   * anything else (a build from source, a distribution package): there is nothing
//     this program installed that it could replace, so the status line says so.
//
// Every download is checked against the SHA-256 digest GitHub publishes for each release
// asset before anything is run. That catches a corrupt or truncated download; it cannot
// vouch for the release itself, which only code signing does, and the builds are not
// signed yet. elhiba chose in-app installation anyway (2026-09-24), from their own
// repository over HTTPS, over sending every user to a download page.
class UpdateChecker : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(Status status READ status NOTIFY statusChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
	Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
	Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY statusChanged)
	Q_PROPERTY(bool canInstall READ canInstall NOTIFY statusChanged)
	Q_PROPERTY(bool readyToInstall READ readyToInstall NOTIFY statusChanged)
	Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
	Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY statusChanged)
	Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)

	public:
		enum Status
		{
			Idle,
			Checking,
			UpToDate,
			UpdateAvailable,
			Downloading,
			ReadyToInstall,
			Installing,
			Failed
		};
		Q_ENUM(Status)

		explicit UpdateChecker(QObject *parent = nullptr);
		~UpdateChecker() override;

		Status	status() const;
		QString	statusText() const;
		bool	busy() const;
		bool	updateAvailable() const;
		bool	canInstall() const;
		bool	readyToInstall() const;
		qreal	downloadProgress() const;
		QString	latestVersion() const;
		QString	currentVersion() const;

	public slots:
		void	check();

		// Downloads and verifies the latest release when this copy knows how to replace
		// itself. check() starts this by itself; it is public for "Try again".
		void	installUpdate();

		// Installs a downloaded, verified update and restarts into it.
		void	applyUpdate();

	signals:
		void	statusChanged();
		void	downloadProgressChanged();

		// The installer has been started and the app is about to quit to make way for it.
		void	quitting();

	private slots:
		void	onCheckFinished();
		void	onDownloadReadyRead();
		void	onDownloadProgress(qint64 received, qint64 total);
		void	onDownloadFinished();
		void	onAboutToQuit();

	private:
		// How this copy was installed, which decides what an update means for it.
		enum InstallKind
		{
			NotInstallable,
			WindowsInstaller,
			WindowsPortable,
			LinuxAppImage,
			MacDiskImage
		};

		static constexpr int	CheckTimeoutMs = 15000;

		static const char *const	LatestReleaseUrl;

		QNetworkAccessManager	_network;
		QNetworkReply			*_reply = nullptr;
		QNetworkReply			*_download = nullptr;

		QFile				_downloadFile;
		QCryptographicHash	_downloadHash{QCryptographicHash::Sha256};

		InstallKind	_installKind = NotInstallable;
		Status		_status = Idle;
		QString		_errorText;
		QString		_latestVersion;
		QString		_downloadedPath;
		qreal		_downloadProgress = 0.0;

		// The release asset for this platform, if the latest release has one.
		QString	_assetName;
		QString	_assetUrl;
		QString	_assetSha256;

		void	setStatus(Status status, const QString &errorText = QString());
		void	setDownloadProgress(qreal progress);
		void	failDownload(const QString &reason);
		void	pickAsset(const QJsonArray &assets);
		QString	downloadPath() const;
		bool	launch(const QString &path, bool relaunch);

		static InstallKind	detectInstallKind();

		// Compares "1.2.3" style versions numerically, so 1.10.0 is newer than 1.9.0 --
		// which a plain string comparison gets backwards.
		static int		compareVersions(const QString &left, const QString &right);
		static QString	normalise(const QString &tag);
};

#endif
