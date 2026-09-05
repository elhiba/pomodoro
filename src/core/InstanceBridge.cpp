#include "InstanceBridge.hpp"

#include <QDBusConnection>
#include <QDBusInterface>

const char	*InstanceBridge::ServiceName = "org.elhiba.pomodoro";
const char	*InstanceBridge::ObjectPath = "/";

InstanceBridge::InstanceBridge(QObject *parent)
	: QObject(parent)
{
	QDBusConnection	bus = QDBusConnection::sessionBus();

	if (!bus.isConnected())
		return;

	// Order matters: the object has to be on the bus before the name is claimed, or a
	// second process could call in during the gap and find nothing there.
	if (!bus.registerObject(QLatin1String(ObjectPath), this, QDBusConnection::ExportScriptableSlots))
		return;

	_primary = bus.registerService(QLatin1String(ServiceName));
}

bool	InstanceBridge::isPrimary() const
{
	return _primary;
}

void	InstanceBridge::raiseWindow()
{
	emit raiseRequested();
}

bool	InstanceBridge::askRunningInstanceToRaise()
{
	QDBusConnection	bus = QDBusConnection::sessionBus();

	if (!bus.isConnected())
		return false;

	QDBusInterface	running(
		QLatin1String(ServiceName), QLatin1String(ObjectPath),
		QLatin1String(ServiceName), bus);

	if (!running.isValid())
		return false;

	return running.call(QStringLiteral("raiseWindow")).type() != QDBusMessage::ErrorMessage;
}
