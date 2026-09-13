#ifndef STREAM_METADATA_HPP
#define STREAM_METADATA_HPP

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

// What an Icecast, Shoutcast or Zeno stream says about itself: the station name and
// genre from the icy-* response headers, and the track currently playing from the
// in-band StreamTitle block.
//
// This is a second, short-lived connection rather than a look at the player's own.
// QMediaPlayer only surfaces ICY tags on some backends (the Windows one never does),
// and none of them report the in-band title as it changes. Pulling one metadata
// block every so often costs a few kilobytes a minute against a stream that costs
// a megabyte, which is cheap for knowing what is on.
class StreamMetadata : public QObject
{
	Q_OBJECT

	public:
		explicit StreamMetadata(QObject *parent = nullptr);

		QString	stationName() const;
		QString	genre() const;
		QString	title() const;

		bool	isEmpty() const;

	public slots:
		// Begins polling. Calling it again with the same URL while running is a no-op;
		// a different URL forgets everything learnt about the old one.
		void	start(const QUrl &url);

		// Stops polling but keeps what was last read, so a brief reconnect does not
		// blank the label. clear() forgets.
		void	stop();
		void	clear();

	signals:
		void	changed();

		// The health of the connection itself, independent of what it said. A stream
		// player cannot tell a live source from one that silently went away, so this
		// second, short connection is what MusicPlayer leans on to know the difference.
		// succeeded fires once the server answers with a 200; failed when the request
		// could not be made or timed out before any answer.
		void	probeSucceeded();
		void	probeFailed();

	private slots:
		void	poll();
		void	onHeaders();
		void	onReadyRead();
		void	onFinished();

	private:
		// Titles change every few minutes at most; a twenty second lag is invisible and
		// keeps the extra traffic down to roughly a twentieth of the stream itself.
		static constexpr int	PollIntervalMs = 20000;
		static constexpr int	RequestTimeoutMs = 15000;

		// After a failed probe the next one comes quickly, so an outage is confirmed in
		// seconds rather than after another full polling interval.
		static constexpr int	FailRetryMs = 4000;

		// Servers announce a metaint in the low tens of kilobytes. One that claims
		// more than this is never going to send a block worth waiting for.
		static constexpr int	MaximumMetaInt = 256 * 1024;

		QNetworkAccessManager	_network;
		QNetworkReply			*_reply = nullptr;
		QTimer					_pollTimer;
		QTimer					_requestTimer;

		QUrl		_url;
		QByteArray	_buffer;
		int			_metaInt = 0;
		bool		_headersRead = false;

		// Whether the current request got a usable answer, so onFinished knows whether
		// the connection was healthy or fell over.
		bool		_requestSucceeded = false;

		QString	_stationName;
		QString	_genre;
		QString	_title;

		void	readHeaders();
		bool	parseMetadataBlock();
		void	abortRequest();
		void	updateValue(QString &field, const QString &value, bool &changed);

		static QString	decode(const QByteArray &bytes);
};

#endif
