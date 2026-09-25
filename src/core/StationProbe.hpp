#ifndef STATION_PROBE_HPP
#define STATION_PROBE_HPP

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <QtQml/qqmlregistration.h>

class QNetworkReply;

// Checks a link someone wants to add to the radio list, before it is saved: does it
// answer, is it audio rather than a web page, and what does the station call itself.
// The station's own icy-name / icy-description headers fill in the name and note, so
// pasting a link is usually all it takes; a .m3u or .pls playlist is opened and the
// stream inside it checked instead, since that is what the player needs.
//
// Only a link that passes becomes Valid, which is the one state the panel lets be
// saved -- the list never fills up with dead links and half-parsed names.
class StationProbe : public QObject
{
	Q_OBJECT
	QML_ELEMENT

	Q_PROPERTY(State state READ state NOTIFY changed)
	Q_PROPERTY(QString url READ url NOTIFY changed)
	Q_PROPERTY(QString name READ name NOTIFY changed)
	Q_PROPERTY(QString note READ note NOTIFY changed)
	Q_PROPERTY(QString message READ message NOTIFY changed)

	public:
		enum State
		{
			Empty,
			Checking,
			Valid,
			Invalid
		};
		Q_ENUM(State)

		// What a response is, from its Content-Type and, for servers that send a
		// generic type, the path.
		enum Kind
		{
			Audio,
			Playlist,
			WebPage,
			Unknown
		};

		explicit StationProbe(QObject *parent = nullptr);

		State	state() const;

		// The stream to save: the pasted link, or the one found inside a playlist file.
		QString	url() const;
		QString	name() const;
		QString	note() const;

		// Why a link was refused, in words a listener understands.
		QString	message() const;

		// No network involved; public for the tests.
		static Kind	classify(const QByteArray &contentType, const QUrl &url);

		// The first stream address in a .m3u or .pls body, or an empty URL.
		static QUrl	firstPlaylistEntry(const QByteArray &body);

	public slots:
		void	check(const QString &text);
		void	reset();

	signals:
		void	changed();

	private slots:
		void	onHeaders();
		void	onFinished();
		void	onTimeout();

	private:
		static constexpr int	TimeoutMs = 10000;

		// Past this many characters after "Name: ", the rest is a description.
		static constexpr int	MaximumNameTail = 24;

		// A playlist file is a few lines; anything bigger is not one.
		static constexpr int	MaximumPlaylistBytes = 64 * 1024;

		QNetworkAccessManager	_network;
		QNetworkReply			*_reply = nullptr;
		QTimer					_timeout;

		State	_state = Empty;
		QString	_url;
		QString	_name;
		QString	_note;
		QString	_message;

		// Set while the body of a playlist file is being read.
		bool	_readingPlaylist = false;

		// A playlist inside a playlist is followed once, not forever.
		bool	_followedPlaylist = false;

		void	request(const QUrl &url);
		void	finish(State state, const QString &message = QString());
		void	dropReply();
};

#endif
