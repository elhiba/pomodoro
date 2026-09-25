#include "StationProbe.hpp"

#include "YtDlp.hpp"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringDecoder>

namespace
{
	// Same rule as StreamMetadata: UTF-8 when it decodes cleanly, Latin-1 otherwise.
	QString	decode(const QByteArray &bytes)
	{
		QStringDecoder	utf8(QStringDecoder::Utf8);
		QString			text = utf8(bytes);

		return utf8.hasError() ? QString::fromLatin1(bytes).trimmed() : text.trimmed();
	}

	// Servers nobody configured announce "Unspecified description" and the like; an
	// empty note is better than a wrong one.
	QString	meaningful(const QString &text)
	{
		return text.contains(QLatin1String("unspecified"), Qt::CaseInsensitive) ? QString() : text;
	}
}

StationProbe::StationProbe(QObject *parent)
	: QObject(parent)
{
	_timeout.setSingleShot(true);
	_timeout.setInterval(TimeoutMs);

	connect(&_timeout, &QTimer::timeout, this, &StationProbe::onTimeout);
}

StationProbe::State	StationProbe::state() const
{
	return _state;
}

QString	StationProbe::url() const
{
	return _url;
}

QString	StationProbe::name() const
{
	return _name;
}

QString	StationProbe::note() const
{
	return _note;
}

QString	StationProbe::message() const
{
	return _message;
}

StationProbe::Kind	StationProbe::classify(const QByteArray &contentType, const QUrl &url)
{
	QByteArray	type = contentType.toLower();
	QString		path = url.path().toLower();

	int	semicolon = type.indexOf(';');

	if (semicolon >= 0)
		type.truncate(semicolon);

	type = type.trimmed();

	// HLS is a playlist by type, but the player opens it directly, so it counts as audio.
	if (type == "application/vnd.apple.mpegurl" || type == "application/x-mpegurl"
		|| path.endsWith(QLatin1String(".m3u8")))
	{
		return Audio;
	}

	if (type == "audio/x-mpegurl" || type == "audio/mpegurl" || type == "audio/x-scpls"
		|| type == "application/pls+xml" || path.endsWith(QLatin1String(".m3u"))
		|| path.endsWith(QLatin1String(".pls")))
	{
		return Playlist;
	}

	if (type.startsWith("audio/") || type == "application/ogg" || type == "video/mp2t"
		|| type == "application/aacp")
	{
		return Audio;
	}

	if (type == "text/html" || type == "application/xhtml+xml")
		return WebPage;

	return Unknown;
}

QUrl	StationProbe::firstPlaylistEntry(const QByteArray &body)
{
	const QList<QByteArray>	lines = body.split('\n');

	for (QByteArray line : lines)
	{
		line = line.trimmed();

		// .pls says "File1=http://..."; .m3u just lists the address.
		int	equals = line.indexOf('=');

		if (line.toLower().startsWith("file") && equals > 0)
			line = line.mid(equals + 1).trimmed();

		QUrl	url(QString::fromUtf8(line));

		if (url.isValid() && !url.host().isEmpty() && (url.scheme() == QLatin1String("http")
			|| url.scheme() == QLatin1String("https")))
		{
			return url;
		}
	}

	return QUrl();
}

void	StationProbe::check(const QString &text)
{
	QString	trimmed = text.trimmed();

	dropReply();

	_followedPlaylist = false;
	_name.clear();
	_note.clear();
	_url.clear();

	if (trimmed.isEmpty())
	{
		finish(Empty);
		return;
	}

	QUrl	url = QUrl::fromUserInput(trimmed);

	if (!url.isValid() || url.host().isEmpty() || (url.scheme() != QLatin1String("http")
		&& url.scheme() != QLatin1String("https")))
	{
		finish(Invalid, tr("Paste a web address that starts with http:// or https://."));
		return;
	}

	if (YtDlp::handles(url))
	{
		finish(Invalid, tr("That is a YouTube link. Pick YouTube at the top to play it."));
		return;
	}

	if (url.host().endsWith(QLatin1String("spotify.com")))
	{
		finish(Invalid, tr("That is a Spotify link. Pick Spotify at the top to play it."));
		return;
	}

	request(url);
}

void	StationProbe::reset()
{
	check(QString());
}

void	StationProbe::request(const QUrl &url)
{
	_url = url.toString();
	_readingPlaylist = false;

	QNetworkRequest	request(url);

	// Asked for so the server answers with its icy-* headers, same as StreamMetadata.
	request.setRawHeader("Icy-MetaData", "1");
	request.setRawHeader("User-Agent",
		(QCoreApplication::applicationName() + QLatin1Char('/')
			+ QCoreApplication::applicationVersion()).toLatin1());
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	_reply = _network.get(request);

	connect(_reply, &QNetworkReply::metaDataChanged, this, &StationProbe::onHeaders);
	connect(_reply, &QNetworkReply::finished, this, &StationProbe::onFinished);

	_timeout.start();

	if (_state != Checking)
	{
		_state = Checking;
		_message.clear();

		emit changed();
	}
}

void	StationProbe::onHeaders()
{
	if (!_reply || _readingPlaylist)
		return;

	int	status = _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	// Redirect hops go by on their way to the real answer.
	if (status >= 300 && status < 400)
		return;

	if (status >= 400)
	{
		finish(Invalid, tr("The server answered with an error (%1). Check the link.").arg(status));
		return;
	}

	QUrl	finalUrl = _reply->url();
	Kind	kind = classify(_reply->header(QNetworkRequest::ContentTypeHeader).toByteArray(), finalUrl);

	// Old Shoutcast servers send no Content-Type at all; an icy-name is as good a sign
	// of a radio stream as any.
	if (kind == Unknown && (_reply->hasRawHeader("icy-name") || _reply->hasRawHeader("icy-metaint")))
		kind = Audio;

	switch (kind)
	{
		case Audio:
		{
			_name = meaningful(decode(_reply->rawHeader("icy-name")));

			QString	description = meaningful(decode(_reply->rawHeader("icy-description")));
			QString	genre = meaningful(decode(_reply->rawHeader("icy-genre")));
			QString	bitrate = decode(_reply->rawHeader("icy-br")).section(QLatin1Char(','), 0, 0);

			if (description == _name)
				description.clear();

			_note = !description.isEmpty() ? description : genre;

			// The one number a listener can hear the difference in.
			if (bitrate.toInt() > 0)
			{
				bitrate += QStringLiteral(" kbps");
				_note = _note.isEmpty() ? bitrate : _note + QStringLiteral(" · ") + bitrate;
			}

			// Some stations put their whole blurb in the name: "Groove Salad: a nicely
			// chilled plate of ambient beats and grooves. [SomaFM]". A long tail after a
			// colon is a description, and goes to the note; a short one ("Radio Paradise:
			// Mellow Mix") is part of the name.
			// Trailing tags go too: "[SomaFM]", "(128k aac)".
			static const QRegularExpression	tags(QStringLiteral(R"((\s*(\[[^\]]*\]|\(\d+\s*k[^)]*\)))+\s*$)"),
				QRegularExpression::CaseInsensitiveOption);

			_name.remove(tags);

			int	colon = _name.indexOf(QLatin1String(": "));

			if (colon > 0 && _name.size() - colon > MaximumNameTail)
			{
				if (description.isEmpty())
					_note = _name.mid(colon + 2).trimmed() + (_note.isEmpty() ? QString() : QStringLiteral(" · ") + _note);

				_name.truncate(colon);
			}

			// No name announced: the site's name ("somafm", "zeno") is a fair start,
			// and the field stays editable.
			if (_name.isEmpty())
				_name = finalUrl.host().section(QLatin1Char('.'), -2, -2);

			// _url stays the address that was asked for, not where it redirected to:
			// Zeno and others redirect to a signed edge URL that expires in a minute.
			finish(Valid);
			return;
		}

		case Playlist:
			if (_followedPlaylist)
			{
				finish(Invalid, tr("That playlist only points to another playlist."));
				return;
			}

			// The few lines of the file are read in onFinished.
			_readingPlaylist = true;
			return;

		case WebPage:
			finish(Invalid, tr("That is a web page, not a stream. Look on the station's site "
				"for its direct stream link, often ending in .mp3, .aac, .m3u or /stream."));
			return;

		case Unknown:
			finish(Invalid, tr("That link does not play audio."));
			return;
	}
}

void	StationProbe::onFinished()
{
	if (!_reply)
		return;

	if (_readingPlaylist)
	{
		bool		failed = _reply->error() != QNetworkReply::NoError;
		QByteArray	body = _reply->read(MaximumPlaylistBytes);

		dropReply();

		QUrl	entry = failed ? QUrl() : firstPlaylistEntry(body);

		if (entry.isEmpty())
		{
			finish(Invalid, tr("That playlist file has no stream in it."));
			return;
		}

		_followedPlaylist = true;
		request(entry);
		return;
	}

	// Finished before any usable headers: the request itself failed.
	QString	error = _reply->errorString();

	dropReply();
	finish(Invalid, tr("Could not reach that link: %1").arg(error));
}

void	StationProbe::onTimeout()
{
	finish(Invalid, tr("That link took too long to answer."));
}

void	StationProbe::finish(State state, const QString &message)
{
	dropReply();

	if (state != Valid)
	{
		_name.clear();
		_note.clear();

		if (state == Empty)
			_url.clear();
	}

	_state = state;
	_message = message;

	emit changed();
}

void	StationProbe::dropReply()
{
	_timeout.stop();

	if (!_reply)
		return;

	// Disconnected first: abort() would otherwise report back into onFinished.
	QNetworkReply	*reply = _reply;

	_reply = nullptr;
	reply->disconnect(this);
	reply->abort();
	reply->deleteLater();
}
