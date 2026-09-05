#ifndef INSTANCE_BRIDGE_HPP
#define INSTANCE_BRIDGE_HPP

#include <QObject>

#include <QtQml/qqmlregistration.h>

// Lets a second copy of the app hand control back to the one already running.
//
// Without this, launching pomodoro while it is minimised does nothing visible: the lock
// file check turns the new process away and the existing window stays hidden. The second
// process now asks the first to show itself before standing down.
class InstanceBridge : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	// Fixed name so the second process knows what to call. Registering the object with
	// ExportScriptableSlots is what makes raiseWindow visible under it.
	Q_CLASSINFO("D-Bus Interface", "org.elhiba.pomodoro")

	public:
		static const char	*ServiceName;
		static const char	*ObjectPath;

		explicit InstanceBridge(QObject *parent = nullptr);

		// True when this process owns the bus name, which only the first one does.
		bool	isPrimary() const;

		// Called by a second process that is about to exit. Returns false if nothing
		// answered, in which case there is nothing running to raise.
		static bool	askRunningInstanceToRaise();

	public slots:
		Q_SCRIPTABLE void	raiseWindow();

	signals:
		// The window listens for this and brings itself to the front.
		void	raiseRequested();

	private:
		bool	_primary = false;
};

#endif
