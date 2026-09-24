#ifndef YT_DLP_HPP
#define YT_DLP_HPP

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <QtQml/qqmlregistration.h>

class QNetworkReply;

// Turns a YouTube link into something QMediaPlayer can play, by asking yt-dlp
// (github.com/yt-dlp/yt-dlp) for the direct URL of its best audio-only stream -- an HLS
// playlist for a live stream, a plain file URL for a video. Those URLs expire after a few
// hours, so MusicPlayer asks again on every (re)connect rather than keeping one.
//
// yt-dlp is a separate program and is not bundled: YouTube changes often enough that a
// copy frozen into a release would stop working within weeks. One on PATH is used as is.
// Otherwise install() fetches the official standalone build for this platform from
// yt-dlp's GitHub releases into the app's data directory, checked against the
// SHA2-256SUMS file published with it, and that copy keeps itself current with its own
// -U, at most once a week.
class YtDlp : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Reached through MusicPlayer.youtube")

	Q_PROPERTY(bool available READ available NOTIFY stateChanged)
	Q_PROPERTY(bool installing READ installing NOTIFY stateChanged)
	Q_PROPERTY(qreal installProgress READ installProgress NOTIFY installProgressChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

	// What the music panel lists: search results or the videos of an opened playlist, as
	// maps with kind ("video" or "playlist"), url, title, subtitle, image and live.
	Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY resultsChanged)
	Q_PROPERTY(QString resultsError READ resultsError NOTIFY resultsChanged)
	Q_PROPERTY(QString resultsTitle READ resultsTitle NOTIFY resultsChanged)

	public:
		explicit YtDlp(QObject *parent = nullptr);
		~YtDlp() override;

		bool	available() const;
		bool	installing() const;
		qreal	installProgress() const;
		QString	statusText() const;

		QVariantList	results() const;
		bool			searching() const;
		QString			resultsError() const;
		QString			resultsTitle() const;

		// True for the hosts yt-dlp is asked about: youtube.com, youtu.be, music.youtube.com.
		static bool	handles(const QUrl &url);

		// Asynchronous; answers with resolved() or resolveFailed(). A second call cancels
		// the first.
		void	resolve(const QUrl &url);
		void	cancel();

		// Lets a copy this app installed update itself, if it has not for a week.
		void	updateIfStale();

	public slots:
		void	install();

		// Searches YouTube for videos, or for playlists when asked.
		void	search(const QString &query, bool playlists);

		// Lists the videos of a playlist in results.
		void	openPlaylist(const QString &url, const QString &title);

	signals:
		void	stateChanged();
		void	installProgressChanged();

		void	resolved(const QUrl &stream, const QString &title, const QString &channel, bool live,
					const QString &thumbnail);
		void	resultsChanged();
		void	resolveFailed(const QString &reason);

	private slots:
		void	onResolveFinished(int exitCode, QProcess::ExitStatus exitStatus);
		void	onBrowseFinished(int exitCode, QProcess::ExitStatus exitStatus);
		void	onSumsFinished();
		void	onBinaryReadyRead();
		void	onBinaryFinished();

	private:
		static constexpr int	ResolveTimeoutMs = 60000;
		static constexpr int	SelfUpdateDays = 7;

		// Enough to choose from without waiting: a search listing of playlists runs to
		// hundreds of entries, and fetching them all took 18 s where 15 take a few.
		static constexpr int	SearchResults = 15;
		static constexpr int	PlaylistEntries = 100;

		QProcess				*_resolver = nullptr;
		QProcess				*_updater = nullptr;
		QProcess				*_browser = nullptr;
		QNetworkAccessManager	_network;
		QNetworkReply			*_download = nullptr;
		QFile					_downloadFile;
		QCryptographicHash		_downloadHash{QCryptographicHash::Sha256};

		QString	_program;
		QString	_expectedSha256;
		QString	_errorText;
		qreal	_installProgress = 0.0;

		QVariantList	_results;
		QString			_resultsError;
		QString			_resultsTitle;
		QString			_pendingTitle;

		void	locate();
		void	browse(const QStringList &arguments, const QString &title);
		void	failInstall(const QString &reason);
		void	prepare(QProcess *process) const;

		// The file yt-dlp publishes for this OS and CPU, e.g. "yt-dlp.exe", "yt-dlp_macos".
		static QString	assetName();
		static QString	managedPath();
};

#endif
