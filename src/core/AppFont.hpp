#ifndef APP_FONT_HPP
#define APP_FONT_HPP

#include <QObject>
#include <QString>

class AppSettings;
class QQuickItem;

// Puts the font chosen in the settings (AppSettings::appFont) on the whole app, as it
// changes, without a restart.
//
// Qt Quick text takes the application font once, when it is created: changing
// QGuiApplication's font afterwards only reaches text created later. So on a change this
// sets the application font for whatever comes next and walks every window's items,
// moving each one still on the previous family to the new one. Anything that chose a font
// of its own is left alone, and a subtree whose root has `property bool ownFont: true`
// (the timer's digits, which follow the setting through their own binding, and the
// "Pomodoro" title) is skipped whole, so their bindings are never overwritten.
class AppFont : public QObject
{
	Q_OBJECT

	public:
		// systemFamily is the font the app started with before any choice was applied:
		// what "no choice" goes back to.
		AppFont(AppSettings *settings, const QString &systemFamily, QObject *parent = nullptr);

		// Applied once at start-up, before the first window is built, so nothing needs
		// walking then.
		static void	applyAtStartup(const QString &family);

	private:
		AppSettings	*_settings = nullptr;
		QString		_systemFamily;
		QString		_current;

		void	apply();

		static void	retune(QQuickItem *item, const QString &from, const QString &to);
};

#endif
