#ifndef NOTIFIER_HPP
#define NOTIFIER_HPP

#include <QObject>
#include <QString>
#include <QVariant>

#include <QtQml/qqmlregistration.h>

// Desktop notifications over the freedesktop D-Bus interface.
//
// The obvious alternative, QSystemTrayIcon::showMessage, drags in Qt Widgets and needs a
// system tray, which modern GNOME does not provide without an extension. Talking to
// org.freedesktop.Notifications directly works on GNOME, KDE and Wayland alike and keeps
// this a Qt Quick application.
class Notifier : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	// False when no notification service answered, so the UI can avoid promising
	// something the desktop will not deliver.
	Q_PROPERTY(bool available READ available CONSTANT)

	public:
		explicit Notifier(QObject *parent = nullptr);

		bool	available() const;

	public slots:
		void	notify(const QString &title, const QString &body);

	private:
		void	prepareIcon();

	private:
		bool	_available = false;

		// Absolute path to a real file on disk, extracted from the resources once.
		QString	_iconPath;

		// The same icon as raw pixels, built once, for daemons that want the image
		// itself rather than somewhere to look it up.
		QVariant	_iconPixels;

		// Replacing the previous notification stops a long session from leaving a
		// stack of stale popups behind.
		unsigned int	_lastId = 0;
};

#endif
