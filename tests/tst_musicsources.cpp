#include <QTest>

#include "SpotifyClient.hpp"
#include "StationProbe.hpp"
#include "YtDlp.hpp"

// The parts of the YouTube, Spotify and station sources that decide what a pasted link
// means. Everything past that talks to yt-dlp, Spotify or a radio server and is checked
// by hand.
class TestMusicSources : public QObject
{
	Q_OBJECT

	private slots:
		void	youtubeHosts_data();
		void	youtubeHosts();
		void	spotifyLinks_data();
		void	spotifyLinks();
		void	stationKinds_data();
		void	stationKinds();
		void	playlistEntries_data();
		void	playlistEntries();
};

void	TestMusicSources::youtubeHosts_data()
{
	QTest::addColumn<QString>("url");
	QTest::addColumn<bool>("handled");

	QTest::newRow("watch") << "https://www.youtube.com/watch?v=abc" << true;
	QTest::newRow("short") << "https://youtu.be/abc" << true;
	QTest::newRow("mobile") << "https://m.youtube.com/watch?v=abc" << true;
	QTest::newRow("music") << "https://music.youtube.com/watch?v=abc" << true;
	QTest::newRow("channel live") << "https://www.youtube.com/@LofiGirl/live" << true;
	QTest::newRow("radio") << "https://stream.zeno.fm/f3wvbbqmdg8uv" << false;
	QTest::newRow("lookalike") << "https://notyoutube.com/watch?v=abc" << false;
}

void	TestMusicSources::youtubeHosts()
{
	QFETCH(QString, url);
	QFETCH(bool, handled);

	QCOMPARE(YtDlp::handles(QUrl(url)), handled);
}

void	TestMusicSources::spotifyLinks_data()
{
	QTest::addColumn<QString>("text");
	QTest::addColumn<QString>("uri");

	QTest::newRow("playlist link") << "https://open.spotify.com/playlist/37i9dQZF1DWWQRwui0ExPn?si=abc"
		<< "spotify:playlist:37i9dQZF1DWWQRwui0ExPn";
	QTest::newRow("localised") << "https://open.spotify.com/intl-fr/album/4aawyAB9vmqN3uQ7FjRGTy"
		<< "spotify:album:4aawyAB9vmqN3uQ7FjRGTy";
	QTest::newRow("track") << "https://open.spotify.com/track/6rqhFgbbKwnb9MLmUQDhG6" << "spotify:track:6rqhFgbbKwnb9MLmUQDhG6";
	QTest::newRow("uri as is") << "spotify:playlist:abc" << "spotify:playlist:abc";
	QTest::newRow("padded") << "  spotify:artist:xyz  " << "spotify:artist:xyz";
	QTest::newRow("user page") << "https://open.spotify.com/user/someone" << "";
	QTest::newRow("other site") << "https://example.com/playlist/abc" << "";
	QTest::newRow("broken uri") << "spotify:playlist" << "";
}

void	TestMusicSources::spotifyLinks()
{
	QFETCH(QString, text);
	QFETCH(QString, uri);

	QCOMPARE(SpotifyClient::toUri(text), uri);
}

void	TestMusicSources::stationKinds_data()
{
	QTest::addColumn<QByteArray>("contentType");
	QTest::addColumn<QString>("url");
	QTest::addColumn<int>("kind");

	QTest::newRow("mp3") << QByteArray("audio/mpeg") << "https://ice1.somafm.com/fluid-128-mp3" << int(StationProbe::Audio);
	QTest::newRow("aac with charset") << QByteArray("audio/aacp; charset=utf-8") << "https://x.fm/live" << int(StationProbe::Audio);
	QTest::newRow("ogg") << QByteArray("application/ogg") << "https://x.fm/live.ogg" << int(StationProbe::Audio);
	QTest::newRow("hls") << QByteArray("application/vnd.apple.mpegurl") << "https://x.fm/live" << int(StationProbe::Audio);
	QTest::newRow("hls by path") << QByteArray("text/plain") << "https://x.fm/live.m3u8" << int(StationProbe::Audio);
	QTest::newRow("m3u") << QByteArray("audio/x-mpegurl") << "https://x.fm/listen" << int(StationProbe::Playlist);
	QTest::newRow("pls by path") << QByteArray("application/octet-stream") << "https://x.fm/listen.pls" << int(StationProbe::Playlist);
	QTest::newRow("web page") << QByteArray("text/html; charset=UTF-8") << "https://x.fm/" << int(StationProbe::WebPage);
	QTest::newRow("json") << QByteArray("application/json") << "https://x.fm/api" << int(StationProbe::Unknown);
}

void	TestMusicSources::stationKinds()
{
	QFETCH(QByteArray, contentType);
	QFETCH(QString, url);
	QFETCH(int, kind);

	QCOMPARE(int(StationProbe::classify(contentType, QUrl(url))), kind);
}

void	TestMusicSources::playlistEntries_data()
{
	QTest::addColumn<QByteArray>("body");
	QTest::addColumn<QString>("url");

	QTest::newRow("m3u") << QByteArray("#EXTM3U\n#EXTINF:-1,Station\nhttps://x.fm/stream.mp3\n")
		<< "https://x.fm/stream.mp3";
	QTest::newRow("pls") << QByteArray("[playlist]\r\nNumberOfEntries=1\r\nFile1=http://x.fm:8000/live\r\nTitle1=X\r\n")
		<< "http://x.fm:8000/live";
	QTest::newRow("empty") << QByteArray("#EXTM3U\n") << "";
	QTest::newRow("html") << QByteArray("<html><body>nope</body></html>") << "";
}

void	TestMusicSources::playlistEntries()
{
	QFETCH(QByteArray, body);
	QFETCH(QString, url);

	QCOMPARE(StationProbe::firstPlaylistEntry(body).toString(), url);
}

QTEST_GUILESS_MAIN(TestMusicSources)

#include "tst_musicsources.moc"
