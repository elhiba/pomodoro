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
	const char *const	KeyScopes = "spotify/scopes";

	const char *const	AuthorizeUrl = "https://accounts.spotify.com/authorize";
	const char *const	TokenUrl = "https://accounts.spotify.com/api/token";
	const char *const	ApiBase = "https://api.spotify.com";

	// Reading and controlling the player, and reading -- never changing -- the user's
	// playlists, liked songs, recent and most played tracks for the music panel.
	const char *const	Scopes =
		"user-read-playback-state user-modify-playback-state user-read-currently-playing "
		"playlist-read-private user-library-read user-read-recently-played user-top-read";

	// The one scope whose absence says the sign-in predates the library.
	const char *const	LibraryScope = "playlist-read-private";

	// The smallest image that is still sharp in a 56 px row. Spotify lists images largest
	// first.
	QString	pickImage(const QJsonArray &images)
	{
		QString	chosen;

		for (const QJsonValue &value : images)
		{
			QJsonObject	image = value.toObject();
			int			width = image.value(QStringLiteral("width")).toInt();

			if (chosen.isEmpty() || width == 0 || width >= 120)
				chosen = image.value(QStringLiteral("url")).toString();
		}

		return chosen;
	}

	QString	artistNames(const QJsonArray &artists)
	{
		QStringList	names;

		for (const QJsonValue &value : artists)
			names.append(value.toObject().value(QStringLiteral("name")).toString());

		return names.join(QStringLiteral(", "));
	}

	QVariantMap	trackItem(const QJsonObject &track)
	{
		return {
			{ QStringLiteral("kind"), QStringLiteral("track") },
			{ QStringLiteral("uri"), track.value(QStringLiteral("uri")).toString() },
			{ QStringLiteral("title"), track.value(QStringLiteral("name")).toString() },
			{ QStringLiteral("subtitle"), artistNames(track.value(QStringLiteral("artists")).toArray()) },
			{ QStringLiteral("image"), pickImage(track.value(QStringLiteral("album")).toObject()
				.value(QStringLiteral("images")).toArray()) }
		};
	}

	QVariantMap	collectionItem(const QString &kind, const QJsonObject &object, const QString &subtitle)
	{
		return {
			{ QStringLiteral("kind"), kind },
			{ QStringLiteral("uri"), object.value(QStringLiteral("uri")).toString() },
			{ QStringLiteral("title"), object.value(QStringLiteral("name")).toString() },
			{ QStringLiteral("subtitle"), subtitle },
			{ QStringLiteral("image"), pickImage(object.value(QStringLiteral("images")).toArray()) }
		};
	}

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
	_grantedScopes = settings.value(QLatin1String(KeyScopes)).toString();

	_authTimeout.setSingleShot(true);
	_authTimeout.setInterval(AuthTimeoutMs);

	_pollTimer.setInterval(PollIntervalMs);

	connect(&_callbackServer, &QTcpServer::newConnection, this, &SpotifyClient::onCallbackConnection);
	connect(&_authTimeout, &QTimer::timeout, this, [this]()
	{
		finishConnecting(QStringLiteral("Spotify sign-in timed out. Press Connect to try again."));
	});
	connect(&_pollTimer, &QTimer::timeout, this, &SpotifyClient::poll);

	// Whatever was asked to play is not going to: say why instead of loading for ever.
	connect(&_engine, &SpotifyEngine::premiumRequiredFound, this, [this]()
	{
		emit playbackFailed(SpotifyEngine::PremiumRequiredText);
	});

	connect(&_engine, &SpotifyEngine::stateChanged, this, [this]()
	{
		// Signed in: the account's display name comes from the Web API with the player's
		// token, which is also the first check that the token works there.
		if (_engine.ready() && _accountName.isEmpty() && canSearch())
			fetchAccount();

		if (!_engine.ready())
			_accessToken.clear();

		emit stateChanged();
	});

	// Signed in on an earlier run: the player starts now, so it is ready by the time
	// something is played.
	if (_engine.available())
	{
		_pollTimer.setInterval(PlayerPollIntervalMs);
		_engine.start();
	}
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

bool	SpotifyClient::builtInPlayer() const
{
	return _engine.available();
}

bool	SpotifyClient::canSearch() const
{
	return !_engine.available() || (!_refreshToken.isEmpty() && !_clientId.isEmpty());
}

QString	SpotifyClient::likedSongsUri() const
{
	return _engine.username().isEmpty()
		? QString()
		: QStringLiteral("spotify:user:%1:collection").arg(_engine.username());
}

bool	SpotifyClient::connected() const
{
	if (_engine.available())
		return _engine.ready() || _engine.remembered();

	return !_refreshToken.isEmpty();
}

bool	SpotifyClient::connecting() const
{
	if (_engine.available())
		return _connecting || _engine.state() == SpotifyEngine::SigningIn
			|| (_engine.state() == SpotifyEngine::Starting && !_engine.remembered());

	return _connecting;
}

QString	SpotifyClient::accountName() const
{
	return _accountName;
}

QString	SpotifyClient::statusText() const
{
	if (_engine.available())
	{
		switch (_engine.state())
		{
			case SpotifyEngine::SigningIn:
				return QStringLiteral("Sign in on the Spotify page that just opened in your browser…");
			case SpotifyEngine::Starting:
				return QStringLiteral("Starting Spotify…");
			case SpotifyEngine::Failed:
				return _engine.errorText();
			case SpotifyEngine::Ready:
				if (_engine.premiumRequired())
					return SpotifyEngine::PremiumRequiredText;

				return _accountName.isEmpty()
					? QStringLiteral("Signed in to Spotify")
					: QStringLiteral("Signed in to Spotify as %1").arg(_accountName);
			default:
				return _engine.errorText().isEmpty() ? QStringLiteral("Not signed in") : _engine.errorText();
		}
	}

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

QString	SpotifyClient::artUrl() const
{
	return _artUrl;
}

qint64	SpotifyClient::positionMs() const
{
	qint64	position = _positionMs;

	if (_isPlaying && _positionClock.isValid())
		position += _positionClock.elapsed();

	return _durationMs > 0 ? qMin(position, _durationMs) : position;
}

qint64	SpotifyClient::durationMs() const
{
	return _durationMs;
}

void	SpotifyClient::seek(qint64 milliseconds)
{
	// Shown at once; the next poll corrects it if the seek did not take.
	_positionMs = milliseconds;
	_positionClock.restart();

	if (_engine.available())
	{
		_engine.call("POST", QStringLiteral("/player/seek"),
			QJsonObject{ { QStringLiteral("position"), milliseconds } },
			[this](int, const QJsonObject &) { QTimer::singleShot(400, this, &SpotifyClient::playerPoll); });
		return;
	}

	api("PUT", QStringLiteral("/v1/me/player/seek?position_ms=%1").arg(milliseconds), QJsonObject(),
		[this](int, const QJsonObject &, const QString &) { QTimer::singleShot(400, this, &SpotifyClient::poll); });
}

bool	SpotifyClient::currentLiked() const
{
	return _liked;
}

bool	SpotifyClient::hasCurrentTrack() const
{
	return _trackUri.startsWith(QLatin1String("spotify:track:"));
}

void	SpotifyClient::addToQueue(const QString &uri)
{
	if (uri.isEmpty())
		return;

	auto	done = [this](bool ok)
	{
		emit notice(ok ? QStringLiteral("Added to the queue") : QStringLiteral("Spotify would not queue that"));
	};

	if (_engine.available())
	{
		if (!_engine.ready())
		{
			emit notice(QStringLiteral("Play something first, then queue more"));
			return;
		}

		_engine.call("POST", QStringLiteral("/player/add_to_queue"), QJsonObject{ { QStringLiteral("uri"), uri } },
			[done](int status, const QJsonObject &) { done(status >= 200 && status < 300); });
		return;
	}

	api("POST", QStringLiteral("/v1/me/player/queue?uri=") + QString::fromLatin1(QUrl::toPercentEncoding(uri)),
		QJsonObject(), [done](int status, const QJsonObject &, const QString &) { done(status >= 200 && status < 300); });
}

// Spotify's library endpoints since February 2026: PUT/DELETE /me/library and
// GET /me/library/contains, all taking URIs.
void	SpotifyClient::toggleLike()
{
	if (!hasCurrentTrack() || !canSearch())
		return;

	bool	like = !_liked;
	QString	uri = _trackUri;

	// Shown at once; put back if Spotify refuses.
	_liked = like;
	emit nowPlayingChanged();

	api(like ? "PUT" : "DELETE", QStringLiteral("/v1/me/library?uris=") + QString::fromLatin1(QUrl::toPercentEncoding(uri)),
		QJsonObject(), [this, like, uri](int status, const QJsonObject &, const QString &)
		{
			bool	ok = status >= 200 && status < 300;

			if (!ok && uri == _trackUri)
			{
				_liked = !like;
				emit nowPlayingChanged();
			}

			emit notice(!ok ? QStringLiteral("Spotify would not change your Liked Songs")
				: like ? QStringLiteral("Added to Liked Songs") : QStringLiteral("Removed from Liked Songs"));
		});
}

// A new song: find out whether it is liked, where the Web API can be asked.
void	SpotifyClient::setTrackUri(const QString &uri)
{
	if (uri == _trackUri)
		return;

	_trackUri = uri;
	_liked = false;

	if (!hasCurrentTrack() || !canSearch())
		return;

	api("GET", QStringLiteral("/v1/me/library/contains?uris=") + QString::fromLatin1(QUrl::toPercentEncoding(uri)),
		QJsonObject(), [this, uri](int status, const QJsonObject &answer, const QString &)
		{
			if (status != 200 || uri != _trackUri)
				return;

			_liked = answer.value(QStringLiteral("array")).toArray().first().toBool();
			emit nowPlayingChanged();
		});
}

QString	SpotifyClient::contextName() const
{
	return _contextNames.value(_contextUri);
}

// Names the list playing now. Spotify's own name when the player gives one (it often
// does not), else the name it was picked by in the panel; Liked Songs and a song started
// from search have fixed names; anything else -- a playlist started from the phone, or
// before this run -- is looked up once through the public oEmbed endpoint.
void	SpotifyClient::setContext(const QString &uri, const QString &reportedName)
{
	if (!reportedName.isEmpty())
		_contextNames.insert(uri, reportedName);
	else if (uri.endsWith(QLatin1String(":collection")))
		_contextNames.insert(uri, QStringLiteral("Liked Songs"));
	else if (uri.startsWith(QLatin1String("spotify:track:")) || uri.startsWith(QLatin1String("spotify:episode:")))
		_contextNames.insert(uri, QStringLiteral("Search"));

	if (uri == _contextUri)
		return;

	_contextUri = uri;

	emit nowPlayingChanged();

	QStringList	parts = uri.split(QLatin1Char(':'));

	if (uri.isEmpty() || _contextNames.contains(uri) || parts.size() != 3)
		return;

	QUrl		oembed(QStringLiteral("https://open.spotify.com/oembed"));
	QUrlQuery	query;

	query.addQueryItem(QStringLiteral("url"),
		QStringLiteral("https://open.spotify.com/%1/%2").arg(parts.at(1), parts.at(2)));
	oembed.setQuery(query);

	QNetworkReply	*reply = _network.get(QNetworkRequest(oembed));

	connect(reply, &QNetworkReply::finished, this, [this, reply, uri]()
	{
		reply->deleteLater();

		QString	title = QJsonDocument::fromJson(reply->readAll()).object()
			.value(QStringLiteral("title")).toString().trimmed();

		if (title.isEmpty())
			return;

		_contextNames.insert(uri, title);

		if (uri == _contextUri)
			emit nowPlayingChanged();
	});
}

QString	SpotifyClient::openedUri() const
{
	return _openedUri;
}

QString	SpotifyClient::openedTitle() const
{
	return _openedTitle;
}

void	SpotifyClient::openContext(const QString &uri, const QString &title)
{
	if (!_engine.available() || uri.isEmpty())
		return;

	// Kept once, so back goes to the shelf or search that was showing, not to another
	// opened list.
	if (_openedUri.isEmpty())
	{
		_resultsBeforeOpening = _results;
		_errorBeforeOpening = _resultsError;
	}

	_openedUri = uri;
	_openedTitle = title;
	_contextNames.insert(uri, title);
	_results.clear();
	_resultsError.clear();
	_searching = true;

	emit resultsChanged();

	int	generation = ++_resultsGeneration;

	_engine.whenReady([this, uri, generation]()
	{
		if (!_engine.ready())
		{
			_searching = false;
			_resultsError = QStringLiteral("Sign in with Spotify first.");
			emit resultsChanged();
			return;
		}

		fetchContext(uri, generation, 0);
	});
}

void	SpotifyClient::closeContext()
{
	if (_openedUri.isEmpty())
		return;

	++_resultsGeneration;

	_openedUri.clear();
	_openedTitle.clear();
	_results = std::exchange(_resultsBeforeOpening, {});
	_resultsError = _errorBeforeOpening;
	_searching = false;

	emit resultsChanged();
}

void	SpotifyClient::playOpened()
{
	if (!_openedUri.isEmpty())
		play(_openedUri);
}

// The player answers at once with whatever it has: nothing while it is still reading the
// list, then the songs, with names and covers filling in over the next calls. Each answer
// replaces the listing, so the songs appear as they arrive.
void	SpotifyClient::fetchContext(const QString &uri, int generation, int attempt)
{
	QString	path = QStringLiteral("/context/tracks?uri=") + QString::fromLatin1(QUrl::toPercentEncoding(uri));

	_engine.call("GET", path, QJsonObject(), [this, uri, generation, attempt](int status, const QJsonObject &body)
	{
		if (generation != _resultsGeneration)
			return;

		bool	ready = body.value(QStringLiteral("ready")).toBool();
		int		length = body.value(QStringLiteral("length")).toInt();
		int		cached = body.value(QStringLiteral("cached")).toInt();

		if (status == 200 && ready)
		{
			QVariantList	songs;

			for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toArray())
			{
				QJsonObject	entry = value.toObject();
				QJsonObject	track = entry.value(QStringLiteral("track")).toObject();
				QStringList	artists;

				for (const QJsonValue &name : track.value(QStringLiteral("artist_names")).toArray())
					artists.append(name.toString());

				QString	title = track.value(QStringLiteral("name")).toString();

				songs.append(QVariantMap {
					{ QStringLiteral("kind"), QStringLiteral("track") },
					{ QStringLiteral("uri"), entry.value(QStringLiteral("uri")).toString() },
					{ QStringLiteral("context"), uri },
					{ QStringLiteral("title"), title.isEmpty() ? QStringLiteral("…") : title },
					{ QStringLiteral("subtitle"), artists.join(QStringLiteral(", ")) },
					{ QStringLiteral("image"), track.value(QStringLiteral("album_cover_url")).toString() }
				});
			}

			_results = songs;
			_resultsError = songs.isEmpty() ? QStringLiteral("Nothing in here.") : QString();
		}

		bool	done = status == 200 && ready && cached >= length;

		_searching = !done && attempt + 1 < ContextPollAttempts && status != 0 && status < 400;

		if (!done && !_searching && _results.isEmpty())
			_resultsError = QStringLiteral("Spotify did not list this one.");

		emit resultsChanged();

		if (_searching)
		{
			QTimer::singleShot(ContextPollMs, this, [this, uri, generation, attempt]()
			{
				fetchContext(uri, generation, attempt + 1);
			});
		}
	});
}

bool	SpotifyClient::needsReconnect() const
{
	// The built-in player's sign-in covers everything.
	if (_engine.available())
		return false;

	return connected() && !_grantedScopes.split(QLatin1Char(' ')).contains(QLatin1String(LibraryScope));
}

QVariantList	SpotifyClient::results() const
{
	return _results;
}

bool	SpotifyClient::searching() const
{
	return _searching;
}

QString	SpotifyClient::resultsError() const
{
	return _resultsError;
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
	if (_engine.available())
	{
		if (!uri.isEmpty())
		{
			playerCommand(QStringLiteral("/player/play"), QJsonObject{ { QStringLiteral("uri"), uri } }, true);
			return;
		}

		// Nothing chosen: carry on with whatever the player has loaded. A player that has
		// just started has nothing, and resuming nothing "succeeds" in silence, so it
		// starts the user's Liked Songs instead.
		_engine.whenReady([this]()
		{
			if (!_engine.ready())
			{
				playerCommand(QStringLiteral("/player/resume"), QJsonObject(), true);
				return;
			}

			_engine.call("GET", QStringLiteral("/status"), QJsonObject(), [this](int status, const QJsonObject &body)
			{
				bool	loaded = status == 200 && !body.value(QStringLiteral("stopped")).toBool()
					&& body.value(QStringLiteral("track")).isObject();

				if (loaded)
					playerCommand(QStringLiteral("/player/resume"), QJsonObject(), true);
				else
					playerCommand(QStringLiteral("/player/play"), QJsonObject{ { QStringLiteral("uri"),
						QStringLiteral("spotify:user:%1:collection").arg(_engine.username()) } }, true);
			});
		});

		return;
	}

	QJsonObject	body;

	if (uri.startsWith(QLatin1String("spotify:track:")) || uri.startsWith(QLatin1String("spotify:episode:")))
		body.insert(QStringLiteral("uris"), QJsonArray{ uri });
	else if (!uri.isEmpty())
		body.insert(QStringLiteral("context_uri"), uri);

	playOn(body, QString());
}

void	SpotifyClient::next()
{
	if (_engine.available())
	{
		playerCommand(QStringLiteral("/player/next"), QJsonObject(), false);
		return;
	}

	api("POST", QStringLiteral("/v1/me/player/next"), QJsonObject(),
		[this](int, const QJsonObject &, const QString &)
		{
			QTimer::singleShot(600, this, &SpotifyClient::poll);
		});
}

void	SpotifyClient::previous()
{
	if (_engine.available())
	{
		playerCommand(QStringLiteral("/player/prev"), QJsonObject(), false);
		return;
	}

	api("POST", QStringLiteral("/v1/me/player/previous"), QJsonObject(),
		[this](int, const QJsonObject &, const QString &)
		{
			QTimer::singleShot(600, this, &SpotifyClient::poll);
		});
}

void	SpotifyClient::playResult(int index)
{
	if (index < 0 || index >= _results.size())
		return;

	QVariantMap	chosen = _results.at(index).toMap();
	QJsonObject	body;

	// The built-in player starts one URI. A song of an opened playlist plays inside that
	// playlist, so next and previous follow it as in Spotify. A song from search is
	// played and the songs after it in the list are queued behind it.
	if (_engine.available())
	{
		QString	uri = chosen.value(QStringLiteral("uri")).toString();
		QString	context = chosen.value(QStringLiteral("context")).toString();

		if (!context.isEmpty())
		{
			playerCommand(QStringLiteral("/player/play"), QJsonObject{
				{ QStringLiteral("uri"), context },
				{ QStringLiteral("skip_to_uri"), uri } }, true);
			return;
		}

		playerCommand(QStringLiteral("/player/play"), QJsonObject{ { QStringLiteral("uri"), uri } }, true);

		if (chosen.value(QStringLiteral("kind")).toString() != QLatin1String("track"))
		{
			_contextNames.insert(uri, chosen.value(QStringLiteral("title")).toString());
			return;
		}

		QStringList	following;

		for (int row = index + 1; row < _results.size() && following.size() < MaximumQueued; row++)
		{
			QVariantMap	item = _results.at(row).toMap();

			if (item.value(QStringLiteral("kind")).toString() == QLatin1String("track"))
				following.append(item.value(QStringLiteral("uri")).toString());
		}

		playerQueue(following);
		return;
	}

	if (chosen.value(QStringLiteral("kind")).toString() == QLatin1String("track"))
	{
		// Every song in the list, starting from the one tapped, so next and previous walk
		// the list the user is looking at.
		QJsonArray	uris;
		int			position = 0;

		for (int row = 0; row < _results.size(); row++)
		{
			QVariantMap	item = _results.at(row).toMap();

			if (item.value(QStringLiteral("kind")).toString() != QLatin1String("track"))
				continue;

			if (row == index)
				position = uris.size();

			uris.append(item.value(QStringLiteral("uri")).toString());
		}

		body.insert(QStringLiteral("uris"), uris);
		body.insert(QStringLiteral("offset"), QJsonObject{ { QStringLiteral("position"), position } });
	}
	else
		body.insert(QStringLiteral("context_uri"), chosen.value(QStringLiteral("uri")).toString());

	playOn(body, QString());
}

void	SpotifyClient::search(const QString &query)
{
	QString	trimmed = query.trimmed();

	if (trimmed.isEmpty())
		return;

	QUrlQuery	params;

	params.addQueryItem(QStringLiteral("q"), trimmed);
	params.addQueryItem(QStringLiteral("type"), QStringLiteral("track,playlist,album,artist"));
	params.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));

	fetchResults(QStringLiteral("/v1/search?") + params.toString(QUrl::FullyEncoded),
		[](const QJsonObject &body)
		{
			QVariantList	items;

			// Songs first: a search is most often for one. Then the collections.
			for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toObject().value(QStringLiteral("items")).toArray())
				items.append(trackItem(value.toObject()));

			// Spotify pads playlist results with nulls for playlists it will not show.
			for (const QJsonValue &value : body.value(QStringLiteral("playlists")).toObject().value(QStringLiteral("items")).toArray())
			{
				if (value.isObject())
				{
					QJsonObject	playlist = value.toObject();

					items.append(collectionItem(QStringLiteral("playlist"), playlist,
						QStringLiteral("Playlist · ") + playlist.value(QStringLiteral("owner")).toObject()
							.value(QStringLiteral("display_name")).toString()));
				}
			}

			for (const QJsonValue &value : body.value(QStringLiteral("albums")).toObject().value(QStringLiteral("items")).toArray())
			{
				QJsonObject	album = value.toObject();

				items.append(collectionItem(QStringLiteral("album"), album,
					QStringLiteral("Album · ") + artistNames(album.value(QStringLiteral("artists")).toArray())));
			}

			for (const QJsonValue &value : body.value(QStringLiteral("artists")).toObject().value(QStringLiteral("items")).toArray())
				items.append(collectionItem(QStringLiteral("artist"), value.toObject(), QStringLiteral("Artist")));

			return items;
		});
}

void	SpotifyClient::loadLibrary(const QString &section)
{
	if (section == QLatin1String("playlists"))
	{
		fetchResults(QStringLiteral("/v1/me/playlists?limit=50"), [](const QJsonObject &body)
		{
			QVariantList	items;

			for (const QJsonValue &value : body.value(QStringLiteral("items")).toArray())
			{
				QJsonObject	playlist = value.toObject();

				// Spotify renamed "tracks" to "items" in February 2026 and every count read
				// 0; the old name stays as a fallback. A playlist the user neither owns nor
				// collaborates on comes with no count at all, and says whose it is instead.
				QJsonValue	count = playlist.value(QStringLiteral("items")).toObject().value(QStringLiteral("total"));

				if (count.isUndefined())
					count = playlist.value(QStringLiteral("tracks")).toObject().value(QStringLiteral("total"));

				int		songs = count.toInt();
				QString	subtitle = count.isUndefined()
					? QStringLiteral("Playlist · ") + playlist.value(QStringLiteral("owner")).toObject()
						.value(QStringLiteral("display_name")).toString()
					: songs == 1 ? QStringLiteral("1 song") : QStringLiteral("%1 songs").arg(songs);

				items.append(collectionItem(QStringLiteral("playlist"), playlist, subtitle));
			}

			return items;
		});
	}
	else if (section == QLatin1String("liked"))
	{
		fetchResults(QStringLiteral("/v1/me/tracks?limit=50"), [](const QJsonObject &body)
		{
			QVariantList	items;

			for (const QJsonValue &value : body.value(QStringLiteral("items")).toArray())
				items.append(trackItem(value.toObject().value(QStringLiteral("track")).toObject()));

			return items;
		});
	}
	else if (section == QLatin1String("recent"))
	{
		fetchResults(QStringLiteral("/v1/me/player/recently-played?limit=50"), [](const QJsonObject &body)
		{
			QVariantList	items;
			QStringList		seen;

			// The history repeats a song every time it was played; once is enough here.
			for (const QJsonValue &value : body.value(QStringLiteral("items")).toArray())
			{
				QJsonObject	track = value.toObject().value(QStringLiteral("track")).toObject();
				QString		uri = track.value(QStringLiteral("uri")).toString();

				if (seen.contains(uri))
					continue;

				seen.append(uri);
				items.append(trackItem(track));
			}

			return items;
		});
	}
	else if (section == QLatin1String("top"))
	{
		fetchResults(QStringLiteral("/v1/me/top/tracks?limit=50&time_range=short_term"), [](const QJsonObject &body)
		{
			QVariantList	items;

			for (const QJsonValue &value : body.value(QStringLiteral("items")).toArray())
				items.append(trackItem(value.toObject()));

			return items;
		});
	}
}

void	SpotifyClient::fetchResults(const QString &path, std::function<QVariantList(const QJsonObject &)> parse)
{
	int	generation = ++_resultsGeneration;

	_searching = true;
	_resultsError.clear();

	emit resultsChanged();

	api("GET", path, QJsonObject(), [this, generation, parse](int status, const QJsonObject &body, const QString &error)
	{
		if (generation != _resultsGeneration)
			return;

		_searching = false;

		if (status == 200)
		{
			_results = parse(body);

			if (_results.isEmpty())
				_resultsError = QStringLiteral("Nothing here.");
		}
		else
		{
			_results.clear();

			if (status == 403 || status == 401)
				_resultsError = QStringLiteral("Spotify needs one more permission for this. Press Reconnect.");
			else
				_resultsError = status > 0 ? reasonFor(status, body) : error;
		}

		emit resultsChanged();
	});
}

void	SpotifyClient::pause()
{
	if (_engine.available())
	{
		playerCommand(QStringLiteral("/player/pause"), QJsonObject(), false);
		return;
	}

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

void	SpotifyClient::reopenOutput()
{
	if (!_engine.ready())
		return;

	// An empty device means "the default", looked up again as it is opened.
	_engine.call("POST", QStringLiteral("/player/output"), QJsonObject{ { QStringLiteral("device"), QString() } },
		[](int, const QJsonObject &) {});
}

void	SpotifyClient::setPlayerVolume(qreal volume)
{
	if (!_engine.ready())
		return;

	int	steps = qRound(qBound(0.0, volume, 1.0) * 100);

	_engine.call("POST", QStringLiteral("/player/volume"),
		QJsonObject{ { QStringLiteral("volume"), steps } },
		[](int, const QJsonObject &) {});
}

void	SpotifyClient::connectAccount()
{
	if (_engine.available())
	{
		_engine.signIn();
		emit stateChanged();
		return;
	}

	startWebSignIn();
}

void	SpotifyClient::connectSearch()
{
	startWebSignIn();
}

void	SpotifyClient::disconnectSearch()
{
	QSettings	settings;

	settings.remove(QLatin1String(KeyRefreshToken));
	settings.remove(QLatin1String(KeyScopes));

	_refreshToken.clear();
	_accessToken.clear();
	_accessExpiry = QDateTime();
	_grantedScopes.clear();
	_results.clear();
	_resultsError.clear();

	emit resultsChanged();
	emit stateChanged();
}

void	SpotifyClient::lookUpLink(const QString &text)
{
	QString	uri = toUri(text);

	if (uri.isEmpty())
	{
		emit linkLookedUp(QString(), QString(), QString(),
			QStringLiteral("That is not a Spotify link. In Spotify, use Share, then Copy link."));
		return;
	}

	QStringList	parts = uri.split(QLatin1Char(':'));
	QUrl		page(QStringLiteral("https://open.spotify.com/%1/%2").arg(parts.at(1), parts.at(2)));
	QUrl		oembed(QStringLiteral("https://open.spotify.com/oembed"));
	QUrlQuery	query;

	query.addQueryItem(QStringLiteral("url"), page.toString());
	oembed.setQuery(query);

	QNetworkReply	*reply = _network.get(QNetworkRequest(oembed));

	connect(reply, &QNetworkReply::finished, this, [this, reply, uri]()
	{
		reply->deleteLater();

		QJsonObject	body = QJsonDocument::fromJson(reply->readAll()).object();
		QString		title = body.value(QStringLiteral("title")).toString().trimmed();

		if (reply->error() != QNetworkReply::NoError || title.isEmpty())
		{
			emit linkLookedUp(QString(), QString(), QString(),
				QStringLiteral("Spotify does not know that link. Check it and try again."));
			return;
		}

		emit linkLookedUp(uri, title, body.value(QStringLiteral("thumbnail_url")).toString(), QString());
	});
}

void	SpotifyClient::startWebSignIn()
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
	if (_engine.available())
		_engine.signOut();

	// What was playing belongs to the account that just left.
	_track.clear();
	_artist.clear();
	_artUrl.clear();
	_isPlaying = false;

	emit nowPlayingChanged();

	QSettings	settings;

	settings.remove(QLatin1String(KeyRefreshToken));
	settings.remove(QLatin1String(KeyAccountName));

	_refreshToken.clear();
	_accessToken.clear();
	_accessExpiry = QDateTime();
	_accountName.clear();
	_errorText.clear();
	_grantedScopes.clear();
	settings.remove(QLatin1String(KeyScopes));

	_results.clear();
	_resultsError.clear();

	setPolling(false);

	emit resultsChanged();

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

	// The built-in player hands out a token for its own session. Spotify refuses it for
	// the Web API today, so a developer Client ID sign-in, when there is one, is used for
	// lookups instead.
	if (_engine.available() && _refreshToken.isEmpty())
	{
		_engine.whenReady([this]()
		{
			_engine.call("POST", QStringLiteral("/token"), QJsonObject(), [this](int status, const QJsonObject &body)
			{
				_refreshing = false;

				if (status == 200)
				{
					_accessToken = body.value(QStringLiteral("token")).toString();
					_accessExpiry = QDateTime::currentDateTimeUtc().addSecs(PlayerTokenSeconds);
				}
				else
					_errorText = QStringLiteral("Sign in with Spotify first.");

				const QList<std::function<void()>>	waiting = std::exchange(_waitingForToken, {});

				for (const std::function<void()> &next : waiting)
					next();
			});
		});

		return;
	}

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
	// What the user actually agreed to, which can be less than what was asked for.
	QString	scopes = body.value(QStringLiteral("scope")).toString();

	if (!scopes.isEmpty() && scopes != _grantedScopes)
	{
		_grantedScopes = scopes;
		QSettings().setValue(QLatin1String(KeyScopes), scopes);
		emit stateChanged();
	}

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
	if (_engine.available())
	{
		playerPoll();
		return;
	}

	api("GET", QStringLiteral("/v1/me/player/currently-playing?additional_types=episode"), QJsonObject(),
		[this](int status, const QJsonObject &body, const QString &)
		{
			QString	track;
			QString	artist;
			QString	art;
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

				_positionMs = body.value(QStringLiteral("progress_ms")).toInteger();
				_durationMs = item.value(QStringLiteral("duration_ms")).toInteger();
				_positionClock.restart();

				setContext(body.value(QStringLiteral("context")).toObject().value(QStringLiteral("uri")).toString(),
					QString());
				setTrackUri(item.value(QStringLiteral("uri")).toString());

				QJsonArray	images = item.value(QStringLiteral("album")).toObject().value(QStringLiteral("images")).toArray();

				if (images.isEmpty())
					images = item.value(QStringLiteral("images")).toArray();

				art = pickImage(images);
			}
			else if (status != 204)
				return;

			if (track == _track && artist == _artist && playing == _isPlaying && art == _artUrl)
				return;

			_track = track;
			_artist = artist;
			_artUrl = art;
			_isPlaying = playing;

			emit nowPlayingChanged();
		});
}

void	SpotifyClient::playOn(const QJsonObject &body, const QString &deviceId)
{
	QString	path = QStringLiteral("/v1/me/player/play");

	if (!deviceId.isEmpty())
		path += QStringLiteral("?device_id=") + QString::fromLatin1(QUrl::toPercentEncoding(deviceId));

	api("PUT", path, body, [this, body, deviceId](int status, const QJsonObject &reply, const QString &error)
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
				[this, body](int, const QJsonObject &devices, const QString &)
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

					playOn(body, chosen);
				});
			return;
		}

		// "Restriction violated" is Spotify refusing a command that would change nothing, or
		// that it cannot carry out from where the player is. Look at the player before
		// calling it a failure: if it is playing, what was wanted has happened.
		if (status == 403 && reply.value(QStringLiteral("error")).toObject()
			.value(QStringLiteral("reason")).toString() != QLatin1String("PREMIUM_REQUIRED"))
		{
			api("GET", QStringLiteral("/v1/me/player"), QJsonObject(),
				[this, body, status, reply](int playerStatus, const QJsonObject &player, const QString &)
				{
					if (playerStatus == 200 && player.value(QStringLiteral("is_playing")).toBool())
					{
						emit playbackStarted();
						poll();
						return;
					}

					// Asked to resume, and there is nothing to resume.
					if (body.isEmpty())
					{
						emit playbackFailed(QStringLiteral("Spotify has nothing to resume. Pick something to play in the music panel."));
						return;
					}

					emit playbackFailed(reasonFor(status, reply));
				});
			return;
		}

		emit playbackFailed(status > 0 ? reasonFor(status, reply) : error);
	});
}

void	SpotifyClient::playerCommand(const QString &path, const QJsonObject &body, bool startsPlayback)
{
	_engine.whenReady([this, path, body, startsPlayback]()
	{
		if (!_engine.ready())
		{
			if (startsPlayback)
			{
				emit playbackFailed(_engine.errorText().isEmpty()
					? QStringLiteral("Sign in with Spotify in the music panel first.")
					: _engine.errorText());
			}

			return;
		}

		_engine.call("POST", path, body, [this, startsPlayback](int status, const QJsonObject &)
		{
			if (startsPlayback)
			{
				if (status >= 200 && status < 300)
					emit playbackStarted();
				else
					emit playbackFailed(status == 0
						? QStringLiteral("The Spotify player is not answering.")
						: QStringLiteral("Spotify would not play that (%1).").arg(status));
			}

			QTimer::singleShot(700, this, &SpotifyClient::playerPoll);
		});
	});
}

// One at a time, in order: queued in parallel they would land shuffled.
void	SpotifyClient::playerQueue(QStringList uris)
{
	if (uris.isEmpty() || !_engine.ready())
		return;

	QString	uri = uris.takeFirst();

	_engine.call("POST", QStringLiteral("/player/add_to_queue"), QJsonObject{ { QStringLiteral("uri"), uri } },
		[this, uris](int status, const QJsonObject &)
		{
			if (status >= 200 && status < 300)
				playerQueue(uris);
		});
}

void	SpotifyClient::playerPoll()
{
	if (!_engine.ready())
		return;

	_engine.call("GET", QStringLiteral("/status"), QJsonObject(), [this](int status, const QJsonObject &body)
	{
		if (status != 200 && status != 204)
			return;

		QJsonObject	track = body.value(QStringLiteral("track")).toObject();
		QStringList	artists;

		for (const QJsonValue &value : track.value(QStringLiteral("artist_names")).toArray())
			artists.append(value.toString());

		QString	title = track.value(QStringLiteral("name")).toString();
		QString	artist = artists.join(QStringLiteral(", "));
		QString	art = track.value(QStringLiteral("album_cover_url")).toString();
		bool	playing = status == 200 && !track.isEmpty()
			&& !body.value(QStringLiteral("paused")).toBool()
			&& !body.value(QStringLiteral("stopped")).toBool();

		_positionMs = track.value(QStringLiteral("position")).toInteger();
		_durationMs = track.value(QStringLiteral("duration")).toInteger();
		_positionClock.restart();

		setContext(body.value(QStringLiteral("context_uri")).toString(),
			body.value(QStringLiteral("context_name")).toString());
		setTrackUri(track.value(QStringLiteral("uri")).toString());

		if (title == _track && artist == _artist && playing == _isPlaying && art == _artUrl)
			return;

		_track = title;
		_artist = artist;
		_artUrl = art;
		_isPlaying = playing;

		emit nowPlayingChanged();
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
	if (!connected())
	{
		handler(0, QJsonObject(), QStringLiteral("Sign in with Spotify in the music panel first."));
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

		int				status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QJsonDocument	document = QJsonDocument::fromJson(reply->readAll());

		// A few endpoints (/me/library/contains) answer with a bare list; handed on as
		// {"array": [...]} so every handler takes the same shape.
		QJsonObject	answer = document.isArray()
			? QJsonObject{ { QStringLiteral("array"), document.array() } }
			: document.object();

		// The token lapsed early (revoked, clock skew). One fresh token, one retry.
		if (status == 401 && !retried)
		{
			_accessToken.clear();
			api(verb, path, body, handler, true);
			return;
		}

		// Enough to tell what Spotify objected to, and nothing that identifies the account:
		// the path, the status and Spotify's own error text. Never the token.
		if (status >= 400 || status == 0)
		{
			QJsonObject	error = answer.value(QStringLiteral("error")).toObject();

			qInfo("pomodoro: spotify %s %s -> %d %s %s", verb.constData(), qPrintable(path.section(QLatin1Char('?'), 0, 0)),
				status, qPrintable(error.value(QStringLiteral("reason")).toString()),
				qPrintable(error.value(QStringLiteral("message")).toString()));
		}

		handler(status, answer, status == 0 ? QStringLiteral("Could not reach Spotify") : QString());
	});
}

QString	SpotifyClient::reasonFor(int status, const QJsonObject &body)
{
	QJsonObject	error = body.value(QStringLiteral("error")).toObject();
	QString		reason = error.value(QStringLiteral("reason")).toString();

	// Only this reason means the account. Spotify answers 403 for plenty of other things
	// -- "Restriction violated" when the player is already doing what was asked, or has
	// nothing loaded to resume -- and calling all of them a Premium problem once told a
	// Premium user they were not.
	if (reason == QLatin1String("PREMIUM_REQUIRED"))
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
