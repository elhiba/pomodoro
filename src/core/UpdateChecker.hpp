#ifndef UPDATE_CHECKER_HPP
#define UPDATE_CHECKER_HPP

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <QtQml/qqmlregistration.h>

class QNetworkReply;

// Asks GitHub what the newest release is and compares it with the running version.
//
// It reports and links, it does not replace anything on disk. Downloading a new binary
// over the running one is a different kind of program: it needs the update to be signed
// to be worth trusting, a way to restart cleanly, and a story for every platform's
// packaging. Until the builds are signed, sending the user to the release page is the
// honest version -- they can see what they are installing and where it came from.
class UpdateChecker : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(Status status READ status NOTIFY statusChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
	Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
	Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY statusChanged)
	Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY statusChanged)
	Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)

	public:
		enum Status
		{
			Idle,
			Checking,
			UpToDate,
			UpdateAvailable,
			Failed
		};
		Q_ENUM(Status)

		explicit UpdateChecker(QObject *parent = nullptr);

		Status	status() const;
		QString	statusText() const;
		bool	busy() const;
		bool	updateAvailable() const;
		QString	latestVersion() const;
		QString	currentVersion() const;

	public slots:
		void	check();

		// Opens the release in the browser. Nothing is downloaded by the app itself.
		void	openDownloadPage();

	signals:
		void	statusChanged();

	private slots:
		void	onFinished();

	private:
		static constexpr int	TimeoutMs = 15000;

		// Unauthenticated and cheap: one request, only when the user asks for it.
		static const char *const	LatestReleaseUrl;
		static const char *const	ReleasesPageUrl;

		QNetworkAccessManager	_network;
		QNetworkReply			*_reply = nullptr;

		Status	_status = Idle;
		QString	_errorText;
		QString	_latestVersion;
		QString	_releaseUrl;

		void	setStatus(Status status, const QString &errorText = QString());

		// Compares "1.2.3" style versions numerically, so 1.10.0 is newer than 1.9.0 --
		// which a plain string comparison gets backwards.
		static int	compareVersions(const QString &left, const QString &right);
		static QString	normalise(const QString &tag);
};

#endif
