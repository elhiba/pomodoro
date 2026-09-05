#ifndef TRAY_ICON_HPP
#define TRAY_ICON_HPP

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

#include <QtQml/qqmlregistration.h>

class QAction;
class QMenu;

// The system tray entry and its menu.
//
// This is the one place the app depends on Qt Widgets: QSystemTrayIcon and QMenu live
// there and Qt Quick has no equivalent. It also means main() has to construct a
// QApplication rather than a QGuiApplication.
//
// It decides nothing about the timer. The menu entries turn into signals and Main.qml
// wires them to the real objects, so this file knows nothing about PomodoroTimer.
class TrayIcon : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	// Whether the desktop claims to have somewhere to put the icon. Worth treating as
	// optimistic rather than a guarantee; see the note in show().
	Q_PROPERTY(bool available READ available CONSTANT)

	Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
	Q_PROPERTY(QString tooltip READ tooltip WRITE setTooltip NOTIFY tooltipChanged)

	// Drive the menu labels from the window and timer state.
	Q_PROPERTY(bool windowVisible READ windowVisible WRITE setWindowVisible NOTIFY windowVisibleChanged)
	Q_PROPERTY(bool timerRunning READ timerRunning WRITE setTimerRunning NOTIFY timerRunningChanged)

	public:
		explicit TrayIcon(QObject *parent = nullptr);
		~TrayIcon() override;

		bool	available() const;
		bool	visible() const;
		QString	tooltip() const;
		bool	windowVisible() const;
		bool	timerRunning() const;

		void	setVisible(bool visible);
		void	setTooltip(const QString &tooltip);
		void	setWindowVisible(bool windowVisible);
		void	setTimerRunning(bool running);

	signals:
		void	visibleChanged();
		void	tooltipChanged();
		void	windowVisibleChanged();
		void	timerRunningChanged();

		// Everything the menu and the icon can ask for. Main.qml decides what they mean.
		void	showRequested();
		void	hideRequested();
		void	toggleTimerRequested();
		void	skipRequested();
		void	quitRequested();

	private slots:
		void	onActivated(QSystemTrayIcon::ActivationReason reason);

	private:
		QSystemTrayIcon	*_icon = nullptr;
		QMenu			*_menu = nullptr;
		QAction			*_showAction = nullptr;
		QAction			*_toggleAction = nullptr;

		bool	_windowVisible = true;
		bool	_timerRunning = false;

		void	refreshLabels();
};

#endif
