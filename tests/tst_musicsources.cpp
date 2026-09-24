#include <QTest>

#include "SpotifyClient.hpp"
#include "YtDlp.hpp"

// The parts of the YouTube and Spotify sources that decide what a pasted link means.
// Everything past that talks to yt-dlp or to Spotify and is checked by hand.
class TestMusicSources : public QObject
{
	Q_OBJECT

	private slots:
		void	youtubeHosts_data();
		void	youtubeHosts();
		void	spotifyLinks_data();
		void	spotifyLinks();
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

QTEST_GUILESS_MAIN(TestMusicSources)

#include "tst_musicsources.moc"
