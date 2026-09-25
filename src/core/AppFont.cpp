#include "AppFont.hpp"

#include "AppSettings.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVariant>

AppFont::AppFont(AppSettings *settings, const QString &systemFamily, QObject *parent)
	: QObject(parent),
	_settings(settings),
	_systemFamily(systemFamily)
{
	_current = _settings->appFont().isEmpty() ? _systemFamily : _settings->appFont();

	connect(_settings, &AppSettings::appFontChanged, this, &AppFont::apply);
}

void	AppFont::applyAtStartup(const QString &family)
{
	if (family.isEmpty())
		return;

	QFont	font = QGuiApplication::font();

	font.setFamily(family);
	QGuiApplication::setFont(font);
}

void	AppFont::apply()
{
	QString	wanted = _settings->appFont().isEmpty() ? _systemFamily : _settings->appFont();

	if (wanted == _current)
		return;

	// For what is created from now on: delegates, popups, drawers opened later.
	QFont	font = QGuiApplication::font();

	font.setFamily(wanted);
	QGuiApplication::setFont(font);

	// And what is on screen already.
	const QWindowList	windows = QGuiApplication::allWindows();

	for (QWindow *window : windows)
	{
		if (QQuickWindow *quick = qobject_cast<QQuickWindow *>(window))
			retune(quick->contentItem(), _current, wanted);
	}

	_current = wanted;
}

void	AppFont::retune(QQuickItem *item, const QString &from, const QString &to)
{
	if (!item || item->property("ownFont").toBool())
		return;

	// Text, TextInput, TextEdit and every control have a "font" property. Only the ones
	// still on the previous app font are moved: a font chosen on purpose stays.
	QVariant	value = item->property("font");

	if (value.metaType() == QMetaType::fromType<QFont>())
	{
		QFont	font = value.value<QFont>();

		if (font.family() == from)
		{
			font.setFamily(to);
			item->setProperty("font", font);
		}
	}

	const QList<QQuickItem *>	children = item->childItems();

	for (QQuickItem *child : children)
		retune(child, from, to);
}
