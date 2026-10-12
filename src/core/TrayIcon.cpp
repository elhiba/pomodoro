#include "TrayIcon.hpp"

#include <QAction>
#include <QIcon>
#include <QMenu>

namespace
{
	const char *const	IconResource = ":/qt/qml/Pomodoro/assets/icons/pomodoroTray.png";
}

TrayIcon::TrayIcon(QObject *parent)
	: QObject(parent)
{
	if (!QSystemTrayIcon::isSystemTrayAvailable())
		return;

	_menu = new QMenu();

	_showAction = _menu->addAction(QStringLiteral("Hide window"));
	connect(_showAction, &QAction::triggered, this, [this]()
	{
		if (_windowVisible)
			emit hideRequested();
		else
			emit showRequested();
	}, Qt::QueuedConnection);

	_menu->addSeparator();

	_toggleAction = _menu->addAction(QStringLiteral("Start"));
	connect(_toggleAction, &QAction::triggered, this, &TrayIcon::toggleTimerRequested, Qt::QueuedConnection);

	QAction	*skip = _menu->addAction(QStringLiteral("Skip session"));
	connect(skip, &QAction::triggered, this, &TrayIcon::skipRequested, Qt::QueuedConnection);

	_menu->addSeparator();

	QAction	*quit = _menu->addAction(QStringLiteral("Quit pomodoro"));
	connect(quit, &QAction::triggered, this, &TrayIcon::quitRequested, Qt::QueuedConnection);

	_icon = new QSystemTrayIcon(QIcon(QString::fromLatin1(IconResource)), this);
	_icon->setContextMenu(_menu);
	_icon->setToolTip(QStringLiteral("pomodoro"));

	connect(_icon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);

	refreshLabels();
}

TrayIcon::~TrayIcon()
{
	// Detach the native menu before destroying it; the tray icon is a QObject child
	// and would otherwise be destroyed only after this destructor returns.
	if (_icon)
	{
		_icon->hide();
		_icon->setContextMenu(nullptr);
		delete _icon;
	}
	// The menu has no QObject parent, so that it outlives nothing and is cleaned up here.
	delete _menu;
}

bool	TrayIcon::available() const
{
	return _icon != nullptr;
}

bool	TrayIcon::visible() const
{
	return _icon != nullptr && _icon->isVisible();
}

QString	TrayIcon::tooltip() const
{
	return _icon ? _icon->toolTip() : QString();
}

bool	TrayIcon::windowVisible() const
{
	return _windowVisible;
}

bool	TrayIcon::timerRunning() const
{
	return _timerRunning;
}

void	TrayIcon::setVisible(bool visible)
{
	if (!_icon || _icon->isVisible() == visible)
		return;

	_icon->setVisible(visible);

	emit visibleChanged();
}

void	TrayIcon::setTooltip(const QString &tooltip)
{
	if (!_icon || _icon->toolTip() == tooltip)
		return;

	_icon->setToolTip(tooltip);

	emit tooltipChanged();
}

void	TrayIcon::setWindowVisible(bool windowVisible)
{
	if (_windowVisible == windowVisible)
		return;

	_windowVisible = windowVisible;
	refreshLabels();

	emit windowVisibleChanged();
}

void	TrayIcon::setTimerRunning(bool running)
{
	if (_timerRunning == running)
		return;

	_timerRunning = running;
	refreshLabels();

	emit timerRunningChanged();
}

void	TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
	// macOS opens the native menu on mouse press. Do not change a window (and
	// consequently its menu labels) while Cocoa is opening that menu. Its Show/Hide
	// action above runs after the native action callback has returned.
#ifdef Q_OS_MACOS
	Q_UNUSED(reason);
	return;
#else
	// A double click also sends Trigger: handling both toggles twice. Unknown and
	// context-menu events must not change window visibility either.
	if (reason != QSystemTrayIcon::Trigger)
		return;

	QMetaObject::invokeMethod(this, [this]()
	{
		if (_windowVisible)
			emit hideRequested();
		else
			emit showRequested();
	}, Qt::QueuedConnection);
#endif
}

void	TrayIcon::refreshLabels()
{
	if (_showAction)
		_showAction->setText(_windowVisible
			? QStringLiteral("Hide window")
			: QStringLiteral("Show pomodoro"));

	if (_toggleAction)
		_toggleAction->setText(_timerRunning
			? QStringLiteral("Pause")
			: QStringLiteral("Start"));
}
