#ifndef SETTINGS_WIDGET_HPP
#define SETTINGS_WIDGET_HPP

#include <QWidget>
#include "TimerWidget.hpp"
#include <QQuickWidget>

class SettingsWidget : public QWidget
{
	private:
		QQuickWidget *_qmlWidget;
	public:
		SettingsWidget(QWidget *parent);
		void setTimerContext(TimerWidget *timer);
};

#endif
