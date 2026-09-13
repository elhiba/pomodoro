#include "StreamMetadata.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringDecoder>

StreamMetadata::StreamMetadata(QObject *parent)
	: QObject(parent)
{
	_pollTimer.setSingleShot(true);
	_pollTimer.setInterval(PollIntervalMs);

	_requestTimer.setSingleShot(true);
	_requestTimer.setInterval(RequestTimeoutMs);

	connect(&_pollTimer, &QTimer::timeout, this, &StreamMetadata::poll);
	connect(&_requestTimer, &QTimer::timeout, this, &StreamMetadata::abortRequest);
}

QString	StreamMetadata::stationName() const
{
	return _stationName;
}

QString	StreamMetadata::genre() const
{
	return _genre;
}

QString	StreamMetadata::title() const
{
	return _title;
}

bool	StreamMetadata::isEmpty() const
{
	return _stationName.isEmpty() && _genre.isEmpty() && _title.isEmpty();
}

void	StreamMetadata::start(const QUrl &url)
{
	if (_url == url && (_reply || _pollTimer.isActive()))
		return;

	if (_url != url)
	{
		clear();
		_url = url;
	}

	poll();
}

void	StreamMetadata::stop()
{
	_pollTimer.stop();
	abortRequest();
}

void	StreamMetadata::clear()
{
	stop();

	bool	changed = false;

	updateValue(_stationName, QString(), changed);
	updateValue(_genre, QString(), changed);
	updateValue(_title, QString(), changed);

	if (changed)
		emit this->changed();
}

void	StreamMetadata::poll()
{
	if (_reply || !_url.isValid())
		return;

	QNetworkRequest	request(_url);

	// Without this header the server sends plain audio and the title never appears.
	request.setRawHeader("Icy-MetaData", "1");
	request.setRawHeader("User-Agent",
		(QCoreApplication::applicationName() + QLatin1Char('/')
			+ QCoreApplication::applicationVersion()).toLatin1());

	// Zeno and friends bounce the public URL to whichever edge server is free.
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	_buffer.clear();
	_metaInt = 0;
	_headersRead = false;
	_requestSucceeded = false;

	_reply = _network.get(request);

	connect(_reply, &QNetworkReply::metaDataChanged, this, &StreamMetadata::onHeaders);
	connect(_reply, &QNetworkReply::readyRead, this, &StreamMetadata::onReadyRead);
	connect(_reply, &QNetworkReply::finished, this, &StreamMetadata::onFinished);

	_requestTimer.start();
}

void	StreamMetadata::onHeaders()
{
	// Fires for the final response; the redirect hops never carry icy-* headers anyway.
	readHeaders();
}

void	StreamMetadata::onReadyRead()
{
	if (!_reply)
		return;

	// Belt and braces: not every backend announces the headers separately before
	// the first byte of body arrives.
	if (!_headersRead)
		readHeaders();

	if (_metaInt <= 0)
	{
		// No in-band metadata on offer, so there is nothing more to read.
		abortRequest();
		return;
	}

	_buffer.append(_reply->readAll());

	if (parseMetadataBlock())
		abortRequest();
}

void	StreamMetadata::onFinished()
{
	if (!_reply)
		return;

	// Cancelled by us after a clean read counts as a success, not a failure; only a
	// request that never got its answer is a failed probe.
	bool	failed = !_requestSucceeded;

	if (failed && _reply->error() != QNetworkReply::OperationCanceledError)
		qDebug() << "pomodoro: stream probe failed:" << _reply->errorString();

	_requestTimer.stop();
	_reply->deleteLater();
	_reply = nullptr;
	_buffer.clear();

	if (failed)
		emit probeFailed();

	// Keep asking for as long as the caller wants to know; sooner after a failure, so
	// an outage is confirmed quickly rather than a full interval later.
	_pollTimer.start(failed ? FailRetryMs : PollIntervalMs);
}

void	StreamMetadata::readHeaders()
{
	if (!_reply || _headersRead)
		return;

	int	status = _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	// A redirect's headers are not the stream's headers.
	if (status >= 300 && status < 400)
		return;

	_headersRead = true;

	// The server answered: the connection is alive, whatever it goes on to say.
	_requestSucceeded = true;
	emit probeSucceeded();

	bool	changed = false;

	updateValue(_stationName, decode(_reply->rawHeader("icy-name")).trimmed(), changed);
	updateValue(_genre, decode(_reply->rawHeader("icy-genre")).trimmed(), changed);

	bool	ok = false;
	int		metaInt = _reply->rawHeader("icy-metaint").toInt(&ok);

	_metaInt = (ok && metaInt > 0 && metaInt <= MaximumMetaInt) ? metaInt : 0;

	if (changed)
		emit this->changed();
}

// The layout after the headers is: metaint bytes of audio, one length byte, then
// length * 16 bytes of "StreamTitle='...';StreamUrl='...';" padded with NULs, and
// round again. Only the first block is wanted, and it is the current title.
bool	StreamMetadata::parseMetadataBlock()
{
	if (_buffer.size() <= _metaInt)
		return false;

	int	length = static_cast<unsigned char>(_buffer.at(_metaInt)) * 16;

	// A zero-length block means "nothing changed since the last one", which on a fresh
	// connection means the server has no title to offer. Stop waiting for one.
	if (length == 0)
		return true;

	if (_buffer.size() < _metaInt + 1 + length)
		return false;

	QByteArray	block = _buffer.mid(_metaInt + 1, length);

	// Strip the NUL padding before decoding, or the text ends in garbage.
	int	end = block.indexOf('\0');

	if (end >= 0)
		block.truncate(end);

	static const QByteArray	key = "StreamTitle='";

	int	start = block.indexOf(key);

	if (start < 0)
		return true;

	start += key.size();

	// Titles can contain a lone apostrophe, so the closing quote is the one that is
	// followed by the separator, not the first one seen.
	int	close = block.indexOf("';", start);

	if (close < 0)
		close = block.size();

	bool	changed = false;

	updateValue(_title, decode(block.mid(start, close - start)).trimmed(), changed);

	if (changed)
		emit this->changed();

	return true;
}

void	StreamMetadata::abortRequest()
{
	_requestTimer.stop();

	if (!_reply)
		return;

	// abort() emits finished(), which is where the reply is released and the next
	// poll is scheduled.
	_reply->abort();
}

void	StreamMetadata::updateValue(QString &field, const QString &value, bool &changed)
{
	if (field == value)
		return;

	field = value;
	changed = true;
}

// ICY predates any agreement on encodings. Most servers send UTF-8 these days; the
// rest send Latin-1, which is what a failed UTF-8 decode is taken to mean.
QString	StreamMetadata::decode(const QByteArray &bytes)
{
	QStringDecoder	utf8(QStringDecoder::Utf8);
	QString			text = utf8(bytes);

	if (utf8.hasError())
		return QString::fromLatin1(bytes);

	return text;
}
