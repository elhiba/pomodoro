#include "SettingsWidget.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <QQuickWidget>
#include <QQmlContext>

SettingsWidget::SettingsWidget(QWidget *parent) : QWidget(parent)
{
// 1. Create a layout for the main settings widget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); // No margins!

    // 2. Create the QML bridge
    _qmlWidget = new QQuickWidget(this);
    
    // 3. THIS IS CRUCIAL: Force the QML root to match the widget size
    _qmlWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    
    // Optional but safe: Force the C++ widget to expand
    _qmlWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 4. Load the file
    _qmlWidget->setSource(QUrl(QStringLiteral("qrc:/SettingsWidget")));

    // 5. Add it to the layout so it stretches
    layout->addWidget(_qmlWidget);
}

void SettingsWidget::setTimerContext(TimerWidget *timer)
{
    // 1. Give QML the C++ timer and name it "timerLogic"
    _qmlWidget->rootContext()->setContextProperty("timerLogic", timer);

    // 2. NOW load the QML file!
    _qmlWidget->setSource(QUrl(QStringLiteral("qrc:/SettingsWidget")));
}
