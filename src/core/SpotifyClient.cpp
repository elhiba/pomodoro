#include "SpotifyClient.hpp"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

#ifndef POMODORO_SPOTIFY_CLIENT_ID
#define POMODORO_SPOTIFY_CLIENT_ID ""
#endif

namespace
{
	const char *const	KeyRefreshToken = "spotify/refreshToken";
	const char *const	KeyAccountName = "spotify/accountName";

	const char *const	AuthorizeUrl = "https://accounts.spotify.com/authorize";
	const char *const	TokenUrl = "https://accounts.spotify.com/api/token";
	const char *const	ApiBase = "https://api.spotify.com";

	// Reading the player's state and controlling it. Nothing about the library, playlists
	// or the profile beyond the display name /v1/me always returns.
	const char *const	Scopes =
		"user-read-playback-state user-modify-playback-state user-read-currently-playing";

	QByteArray	randomUrlSafe(int bytes)
	{
		QByteArray	raw(bytes, Qt::Uninitialized);

		QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(raw.data()), bytes / 4);

		return raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	}

	// What the browser tab shows after the redirect. Plain and self-contained: no
	// scripts, no external resources.
	QByteArray	callbackPage(const QString &message)
	{
		QByteArray	html = "<!doctype html><html><head><meta charset=\"utf-8\"><title>Pomodoro</title></head>"
			"<body style=\"font-family:sans-serif;background:#ba4949;color:white;text-align:center;padding-top:15vh\">"
			"<h1>Pomodoro</h1><p>" + message.toHtmlEscaped().toUtf8() + "</p></body></html>";

		return "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n"
			"Content-Length: " + QByteArray::number(html.size()) + "\r\n\r\n" + html;
	}
}

SpotifyClient::SpotifyClient(QObject *parent)
	: QObject(parent),
	_clientId(QString::fromUtf8(POMODORO_SPOTIFY_CLIENT_ID).trimmed())
{
	QSettings	settings;

	_refreshToken = settings.value(QLatin1String(KeyRefreshToken)).toString();
	_accountName = settings.value(QLatin1String(KeyAccountName)).toString();

	_authTimeout.setSingleShot(true);
	_authTimeout.setInterval(AuthTimeoutMs);

	_pollTimer.setInterval(PollIntervalMs);

	connect(&_callbackServer, &QTcpServer::newConnection, this, &SpotifyClient::onCallbackConnection);
	connect(&_authTimeout, &QTimer::timeout, this, [this]()
	{
		finishConnecting(QStringLiteral("Spotify sign-in timed out. Press Connect to try again."));
	});
	connect(&_pollTimer, &QTimer::timeout, this, &SpotifyClient::poll);
}

QString	SpotifyClient::clientId() const
{
	return _clientId;
}

QString	SpotifyClient::builtInClientId() const
{
	return QString::fromUtf8(POMODORO_SPOTIFY_CLIENT_ID).trimmed();
}

QString	SpotifyClient::redirectUri() const
{
	return QStringLiteral("http://127.0.0.1:%1/callback").arg(CallbackPort);
}

bool	SpotifyClient::connected() const
{
	return !_refreshToken.isEmpty();
}

bool	SpotifyClient::connecting() const
{
	return _connecting;
}

QString	SpotifyClient::accountName() const
{
	return _accountName;
}

QString	SpotifyClient::statusText() const
{
	if (_connecting)
		return QStringLiteral("Approve Pomodoro in the browser window that just opened…");

	if (!_errorText.isEmpty())
		return _errorText;

	if (connected())
	{
		return _accountName.isEmpty()
			? QStringLiteral("Connected to Spotify")
			: QStringLiteral("Connected to Spotify as %1").arg(_accountName);
	}

	if (_clientId.isEmpty())
		return QStringLiteral("Spotify needs a Client ID before it can be connected (see below).");

	return QStringLiteral("Not connected");
}

QString	SpotifyClient::track() const
{
	return _track;
}

QString	SpotifyClient::artist() const
{
	return _artist;
}

bool	SpotifyClient::isPlaying() const
{
	return _isPlaying;
}

void	SpotifyClient::setClientId(const QString &clientId)
{
	QString	trimmed = clientId.trimmed();

	if (_clientId == trimmed)
		return;

	// Tokens are issued to one client; they mean nothing to another.
	if (!_clientId.isEmpty() && connected())
		disconnectAccount();

	_clientId = trimmed;
	_errorText.clear();

	emit stateChanged();
}

void	SpotifyClient::play(const QString &uri)
{
	playOn(uri, QString());
}

void	SpotifyClient::pause()
{
	api("PUT", QStringLiteral("/v1/me/player/pause"), QJsonObject(),
		[this](int, const QJsonObject &, const QString &)
		{
			// Already paused answers 403, which is the outcome that was wanted anyway.
			poll();
		});
}

void	SpotifyClient::setPolling(bool polling)
{
	if (polling == _pollTimer.isActive())
		return;

	if (polling)
	{
		_pollTimer.start();
		poll();
	}
	else
		_pollTimer.stop();
}

QString	SpotifyClient::toUri(const QString &text)
{
	static const QStringList	kinds = {
		QStringLiteral("playlist"), QStringLiteral("album"), QStringLiteral("artist"),
		QStringLiteral("track"), QStringLiteral("show"), QStringLiteral("episode")
	};

	QString	trimmed = text.trimmed();

	if (trimmed.startsWith(QLatin1String("spotify:")))
	{
		QStringList	parts = trimmed.split(QLatin1Char(':'));

		return parts.size() == 3 && kinds.contains(parts.at(1)) && !parts.at(2).isEmpty()
			? trimmed
			: QString();
	}

	QUrl	url(trimmed);

	if (url.host() != QLatin1String("open.spotify.com"))
		return QString();

	QStringList	segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);

	// Localised links carry a leading "intl-fr" or similar.
	if (!segments.isEmpty() && segments.first().startsWith(QLatin1String("intl-")))
		segments.removeFirst();

	if (segments.size() < 2 || !kinds.contains(segments.at(0)))
		return QString();

	return QStringLiteral("spotify:%1:%2").arg(segments.at(0), segments.at(1));
}

void	SpotifyClient::connectAccount()
{
	if (_connecting)
		return;

	if (_clientId.isEmpty())
	{
		_errorText = QStringLiteral("Enter a Spotify Client ID first.");
		emit stateChanged();
		return;
	}

	if (!_callbackServer.listen(QHostAddress::LocalHost, CallbackPort))
	{
		_errorText = QStringLiteral("Could not listen on port %1 for Spotify's answer: %2")
			.arg(CallbackPort).arg(_callbackServer.errorString());
		emit stateChanged();
		return;
	}

	_verifier = randomUrlSafe(64);
	_state = randomUrlSafe(16);

	QByteArray	challenge = QCryptographicHash::hash(_verifier, QCryptographicHash::Sha256)
		.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

	QUrlQuery	query;

	query.addQueryItem(QStringLiteral("client_id"), _clientId);
	query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
	query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
	query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
	query.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(challenge));
	query.addQueryItem(QStringLiteral("state"), QString::fromLatin1(_state));
	query.addQueryItem(QStringLiteral("scope"), QLatin1String(Scopes));

	QUrl	url{QLatin1String(AuthorizeUrl)};

	url.setQuery(query);

	_connecting = true;
	_errorText.clear();
	_authTimeout.start();

	emit stateChanged();

	QDesktopServices::openUrl(url);
}

void	SpotifyClient::disconnectAccount()
{
	QSettings	settings;

	settings.remove(QLatin1String(KeyRefreshToken));
	settings.remove(QLatin1String(KeyAccountName));

	_refreshToken.clear();
	_accessToken.clear();
	_accessExpiry = QDateTime();
	_accountName.clear();
	_errorText.clear();

	setPolling(false);

	emit stateChanged();
}

void	SpotifyClient::onCallbackConnection()
{
	while (QTcpSocket *socket = _callbackServer.nextPendingConnection())
	{
		connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
		connect(socket, &QTcpSocket::readyRead, this, [this, socket]()
		{
			QByteArray	request = socket->peek(8192);

			if (!request.contains("\r\n\r\n") && request.size() < 8192)
				return;

			socket->readAll();

			// "GET /callback?code=...&state=... HTTP/1.1"
			QList<QByteArray>	line = request.left(request.indexOf("\r\n")).split(' ');
			QUrl				target(QStringLiteral("http://127.0.0.1")
				+ QString::fromLatin1(line.value(1)));

			// Browsers also ask for /favicon.ico; that is not the answer being waited for.
			if (line.value(0) != "GET" || target.path() != QLatin1String("/callback"))
			{
				socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
				socket->disconnectFromHost();
				return;
			}

			QUrlQuery	query(target);
			QString		error = query.queryItemValue(QStringLiteral("error"));
			QByteArray	code = query.queryItemValue(QStringLiteral("code")).toLatin1();
			QByteArray	state = query.queryItemValue(QStringLiteral("state")).toLatin1();

			bool	good = error.isEmpty() && !code.isEmpty() && state == _state;

			socket->write(callbackPage(good
				? QStringLiteral("Spotify is connected. You can close this tab and go back to Pomodoro.")
				: QStringLiteral("Spotify was not connected. You can close this tab.")));
			socket->disconnectFromHost();

			_callbackServer.close();

			if (!good)
			{
				finishConnecting(error == QLatin1String("access_denied")
					? QStringLiteral("Spotify access was not granted.")
					: QStringLiteral("Spotify sign-in did not complete."));
				return;
			}

			exchangeCode(code);
		});
	}
}

void	SpotifyClient::exchangeCode(const QByteArray &code)
{
	QUrlQuery	form;

	form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
	form.addQueryItem(QStringLiteral("code"), QString::fromLatin1(code));
	form.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
	form.addQueryItem(QStringLiteral("client_id"), _clientId);
	form.addQueryItem(QStringLiteral("code_verifier"), QString::fromLatin1(_verifier));

	QNetworkRequest	request((QUrl(QLatin1String(TokenUrl))));

	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

	QNetworkReply	*reply = _network.post(request, form.toString(QUrl::FullyEncoded).toLatin1());

	connect(reply, &QNetworkReply::finished, this, [this, reply]()
	{
		reply->deleteLater();

		QJsonObject	body = QJsonDocument::fromJson(reply->readAll()).object();

		if (reply->error() != QNetworkReply::NoError || !body.contains(QStringLiteral("access_token")))
		{
			finishConnecting(QStringLiteral("Spotify refused the sign-in: %1")
				.arg(body.value(QStringLiteral("error_description")).toString(reply->errorString())));
			return;
		}

		storeTokens(body);
		finishConnecting(QString());
		fetchAccount();
	});
}

void	SpotifyClient::refreshAccessToken(std::function<void()> then)
{
	_waitingForToken.append(std::move(then));

	if (_refreshing)
		return;

	_refreshing = true;

	QUrlQuery	form;

	form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
	form.addQueryItem(QStringLiteral("refresh_token"), _refreshToken);
	form.addQueryItem(QStringLiteral("client_id"), _clientId);

	QNetworkRequest	request((QUrl(QLatin1String(TokenUrl))));

	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

	QNetworkReply	*reply = _network.post(request, form.toString(QUrl::FullyEncoded).toLatin1());

	connect(reply, &QNetworkReply::finished, this, [this, reply]()
	{
		reply->deleteLater();
		_refreshing = false;

		int			status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QJsonObject	body = QJsonDocument::fromJson(reply->readAll()).object();

		if (body.contains(QStringLiteral("access_token")))
			storeTokens(body);
		else if (status == 400)
		{
			// The grant was revoked (from the Spotify account page) or has expired; the
			// user has to sign in again, and saying so beats failing every call quietly.
			disconnectAccount();
			_errorText = QStringLiteral("Spotify signed Pomodoro out. Connect again to carry on.");
			emit stateChanged();
		}

		// Whoever was waiting finds out from the token being there or not.
		const QList<std::function<void()>>	waiting = std::exchange(_waitingForToken, {});

		for (const std::function<void()> &next : waiting)
			next();
	});
}

void	SpotifyClient::storeTokens(const QJsonObject &body)
{
	_accessToken = body.value(QStringLiteral("access_token")).toString();

	// A minute early, so a call never sets off with a token about to lapse in flight.
	int	lifetime = qMax(120, body.value(QStringLiteral("expires_in")).toInt(3600));

	_accessExpiry = QDateTime::currentDateTimeUtc().addSecs(lifetime - 60);

	// Spotify may rotate the refresh token on a refresh; when it does, the old one stops
	// working, so the new one has to be written down straight away.
	QString	refresh = body.value(QStringLiteral("refresh_token")).toString();

	if (!refresh.isEmpty() && refresh != _refreshToken)
	{
		_refreshToken = refresh;
		QSettings().setValue(QLatin1String(KeyRefreshToken), _refreshToken);
	}
}

void	SpotifyClient::fetchAccount()
{
	api("GET", QStringLiteral("/v1/me"), QJsonObject(),
		[this](int status, const QJsonObject &body, const QString &)
		{
			if (status != 200)
				return;

			QString	name = body.value(QStringLiteral("display_name")).toString();

			if (name.isEmpty())
				name = body.value(QStringLiteral("id")).toString();

			_accountName = name;
			QSettings().setValue(QLatin1String(KeyAccountName), name);

			emit stateChanged();
		});
}

void	SpotifyClient::poll()
{
	api("GET", QStringLiteral("/v1/me/player/currently-playing?additional_types=episode"), QJsonObject(),
		[this](int status, const QJsonObject &body, const QString &)
		{
			QString	track;
			QString	artist;
			bool	playing = false;

			if (status == 200)
			{
				QJsonObject	item = body.value(QStringLiteral("item")).toObject();
				QStringList	artists;

				for (const QJsonValue &value : item.value(QStringLiteral("artists")).toArray())
					artists.append(value.toObject().value(QStringLiteral("name")).toString());

				// A podcast episode has a show where a track has artists.
				if (artists.isEmpty())
					artists.append(item.value(QStringLiteral("show")).toObject().value(QStringLiteral("name")).toString());

				track = item.value(QStringLiteral("name")).toString();
				artist = artists.join(QStringLiteral(", "));
				playing = body.value(QStringLiteral("is_playing")).toBool();
			}
			else if (status != 204)
				return;

			if (track == _track && artist == _artist && playing == _isPlaying)
				return;

			_track = track;
			_artist = artist;
			_isPlaying = playing;

			emit nowPlayingChanged();
		});
}

void	SpotifyClient::playOn(const QString &uri, const QString &deviceId)
{
	QJsonObject	body;

	if (uri.startsWith(QLatin1String("spotify:track:")) || uri.startsWith(QLatin1String("spotify:episode:")))
		body.insert(QStringLiteral("uris"), QJsonArray{ uri });
	else if (!uri.isEmpty())
		body.insert(QStringLiteral("context_uri"), uri);

	QString	path = QStringLiteral("/v1/me/player/play");

	if (!deviceId.isEmpty())
		path += QStringLiteral("?device_id=") + QString::fromLatin1(QUrl::toPercentEncoding(deviceId));

	api("PUT", path, body, [this, uri, deviceId](int status, const QJsonObject &reply, const QString &error)
	{
		if (status >= 200 && status < 300)
		{
			emit playbackStarted();
			QTimer::singleShot(800, this, &SpotifyClient::poll);
			return;
		}

		// No player is active. Spotify will start one that is open but idle if asked by
		// name, so look for one before giving up.
		if (status == 404 && deviceId.isEmpty())
		{
			api("GET", QStringLiteral("/v1/me/player/devices"), QJsonObject(),
				[this, uri](int, const QJsonObject &devices, const QString &)
				{
					QString	chosen;

					for (const QJsonValue &value : devices.value(QStringLiteral("devices")).toArray())
					{
						QJsonObject	device = value.toObject();

						if (device.value(QStringLiteral("is_restricted")).toBool())
							continue;

						if (chosen.isEmpty() || device.value(QStringLiteral("is_active")).toBool())
							chosen = device.value(QStringLiteral("id")).toString();
					}

					if (chosen.isEmpty())
					{
						emit playbackFailed(QStringLiteral("Open Spotify on this computer or your phone first; Pomodoro plays through it."));
						return;
					}

					playOn(uri, chosen);
				});
			return;
		}

		emit playbackFailed(status > 0 ? reasonFor(status, reply) : error);
	});
}

void	SpotifyClient::finishConnecting(const QString &error)
{
	_authTimeout.stop();
	_callbackServer.close();

	_connecting = false;
	_errorText = error;
	_verifier.clear();
	_state.clear();

	emit stateChanged();
}

void	SpotifyClient::api(const QByteArray &verb, const QString &path, const QJsonObject &body,
	Handler handler, bool retried)
{
	if (_refreshToken.isEmpty())
	{
		handler(0, QJsonObject(), QStringLiteral("Connect Spotify in Settings, under Music, first."));
		return;
	}

	if (_accessToken.isEmpty() || QDateTime::currentDateTimeUtc() >= _accessExpiry)
	{
		refreshAccessToken([this, verb, path, body, handler, retried]()
		{
			if (_accessToken.isEmpty() || QDateTime::currentDateTimeUtc() >= _accessExpiry)
			{
				handler(0, QJsonObject(), _errorText.isEmpty()
					? QStringLiteral("Could not reach Spotify")
					: _errorText);
				return;
			}

			api(verb, path, body, handler, retried);
		});
		return;
	}

	QNetworkRequest	request(QUrl(QLatin1String(ApiBase) + path));

	request.setRawHeader("Authorization", "Bearer " + _accessToken.toLatin1());

	QByteArray	payload;

	if (!body.isEmpty())
	{
		payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
		request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	}

	QNetworkReply	*reply = verb == "GET"
		? _network.get(request)
		: _network.sendCustomRequest(request, verb, payload);

	connect(reply, &QNetworkReply::finished, this, [this, reply, verb, path, body, handler, retried]()
	{
		reply->deleteLater();

		int			status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QJsonObject	answer = QJsonDocument::fromJson(reply->readAll()).object();

		// The token lapsed early (revoked, clock skew). One fresh token, one retry.
		if (status == 401 && !retried)
		{
			_accessToken.clear();
			api(verb, path, body, handler, true);
			return;
		}

		handler(status, answer, status == 0 ? QStringLiteral("Could not reach Spotify") : QString());
	});
}

QString	SpotifyClient::reasonFor(int status, const QJsonObject &body)
{
	QJsonObject	error = body.value(QStringLiteral("error")).toObject();
	QString		reason = error.value(QStringLiteral("reason")).toString();

	if (reason == QLatin1String("PREMIUM_REQUIRED") || status == 403)
		return QStringLiteral("Spotify only lets other apps control playback on a Premium account.");

	if (reason == QLatin1String("NO_ACTIVE_DEVICE"))
		return QStringLiteral("Open Spotify on this computer or your phone first; Pomodoro plays through it.");

	if (status == 429)
		return QStringLiteral("Spotify is rate limiting, try again in a minute.");

	QString	message = error.value(QStringLiteral("message")).toString();

	return message.isEmpty()
		? QStringLiteral("Spotify answered %1").arg(status)
		: QStringLiteral("Spotify: %1").arg(message);
}
