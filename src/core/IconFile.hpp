#ifndef ICON_FILE_HPP
#define ICON_FILE_HPP

#include <QByteArray>
#include <QFile>
#include <QSaveFile>
#include <QString>

namespace IconFile
{
	// Refresh an old icon without touching an identical file. Atomic replacement keeps
	// readers from seeing a partial PNG and preserves the old icon if writing fails.
	inline bool writeIfChanged(const QString &path, const QByteArray &data)
	{
		if (data.isEmpty())
			return false;

		QFile existing(path);
		if (existing.open(QIODevice::ReadOnly) && existing.readAll() == data)
			return true;
		existing.close();

		QSaveFile destination(path);
		if (!destination.open(QIODevice::WriteOnly))
			return false;
		if (destination.write(data) != data.size())
			return false;
		return destination.commit();
	}

	inline bool refreshResource(const QString &path, const QString &resource)
	{
		QFile source(resource);
		if (!source.open(QIODevice::ReadOnly))
			return false;
		return writeIfChanged(path, source.readAll());
	}
}

#endif
