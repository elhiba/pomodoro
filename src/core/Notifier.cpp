#include "Notifier.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QVariantMap>

// The image-data hint is a (iiibiiay) struct: width, height, rowstride, whether there is
// an alpha channel, bits per sample, channels, and the pixels themselves. Qt has no
// built-in type for it, so it gets declared and marshalled here.
struct NotificationImage
{
	int			width = 0;
	int			height = 0;
	int			rowstride = 0;
	bool		hasAlpha = true;
	int			bitsPerSample = 8;
	int			channels = 4;
	QByteArray	pixels;
};

Q_DECLARE_METATYPE(NotificationImage)

static QDBusArgument	&operator<<(QDBusArgument &argument, const NotificationImage &image)
{
	argument.beginStructure();
	argument << image.width << image.height << image.rowstride << image.hasAlpha
		<< image.bitsPerSample << image.channels << image.pixels;
	argument.endStructure();

	return argument;
}

static const QDBusArgument	&operator>>(const QDBusArgument &argument, NotificationImage &image)
{
	argument.beginStructure();
	argument >> image.width >> image.height >> image.rowstride >> image.hasAlpha
		>> image.bitsPerSample >> image.channels >> image.pixels;
	argument.endStructure();

	return argument;
}

namespace
{
	const char *const	Service = "org.freedesktop.Notifications";
	const char *const	Path = "/org/freedesktop/Notifications";
	const char *const	Interface = "org.freedesktop.Notifications";

	const char *const	IconResource = ":/qt/qml/Pomodoro/assets/icons/pomodoroLogoTransport.png";

	constexpr int	TimeoutMs = 8000;

	// The logo is 500x500. Sending that as raw RGBA would be a megabyte down the bus for
	// every session that ends, and no notification is drawn anywhere near that big.
	constexpr int	IconPixels = 128;
}

Notifier::Notifier(QObject *parent)
	: QObject(parent)
{
	QDBusConnection	bus = QDBusConnection::sessionBus();

	_available = bus.isConnected()
		&& bus.interface()->isServiceRegistered(QLatin1String(Service)).value();

	if (_available)
		prepareIcon();
}

// The app_icon field takes either a themed icon name or a path to a real file. A themed
// name only resolves once the desktop entry and icon are installed, so running from the
// build directory would show no icon at all. Unpacking the resource into the cache once
// means the notification looks right either way.
void	Notifier::prepareIcon()
{
	QString	directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

	if (directory.isEmpty() || !QDir().mkpath(directory))
		return;

	QString	path = directory + QStringLiteral("/pomodoro-notification.png");

	if (!QFile::exists(path))
	{
		QFile	source(QString::fromLatin1(IconResource));

		if (!source.copy(path))
			return;

		// Files copied out of the resources inherit the read only bit, which would stop
		// a later version of the icon from ever replacing this one.
		QFile::setPermissions(path,
			QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
	}

	_iconPath = path;

	// GNOME Shell, which is the notification daemon here, ignores the app_icon argument
	// entirely: it resolves the icon from the desktop-entry hint, which only works once
	// the application is installed, or from image-data. Running from the build directory
	// therefore showed no icon at all. Sending the pixels works either way.
	qDBusRegisterMetaType<NotificationImage>();

	QImage	image(QString::fromLatin1(IconResource));

	if (image.isNull())
		return;

	image = image
		.scaled(IconPixels, IconPixels, Qt::KeepAspectRatio, Qt::SmoothTransformation)
		.convertToFormat(QImage::Format_RGBA8888);

	NotificationImage	payload;

	payload.width = image.width();
	payload.height = image.height();
	payload.rowstride = static_cast<int>(image.bytesPerLine());
	payload.pixels = QByteArray(
		reinterpret_cast<const char *>(image.constBits()),
		static_cast<qsizetype>(image.sizeInBytes()));

	_iconPixels = QVariant::fromValue(payload);
}

bool	Notifier::available() const
{
	return _available;
}

void	Notifier::notify(const QString &title, const QString &body)
{
	if (!_available)
		return;

	QDBusInterface	notifications(
		QLatin1String(Service), QLatin1String(Path), QLatin1String(Interface),
		QDBusConnection::sessionBus());

	if (!notifications.isValid())
		return;

	QVariantMap	hints;

	// Lets the desktop pair the notification with the installed application entry.
	hints.insert(QStringLiteral("desktop-entry"), QCoreApplication::applicationName());

	// The alarm has already played by the time this is sent; letting the desktop add
	// its own chime on top would just be two noises for one event.
	hints.insert(QStringLiteral("suppress-sound"), true);

	if (_iconPixels.isValid())
		hints.insert(QStringLiteral("image-data"), _iconPixels);

	QDBusReply<unsigned int>	reply = notifications.call(
		QStringLiteral("Notify"),
		QCoreApplication::applicationName(),
		_lastId,
		_iconPath.isEmpty() ? QStringLiteral("pomodoro") : _iconPath,
		title,
		body,
		QStringList(),
		hints,
		TimeoutMs);

	if (reply.isValid())
		_lastId = reply.value();
}
