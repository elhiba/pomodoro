#include "PomodoroWindow.hpp"
#include "TimerWidget.hpp"

#include <QFontDatabase>
#include <QFont>
#include <iostream>

#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QResizeEvent>


PomodoroWindow::PomodoroWindow()
{
	this->setStyleSheet("background-color: #12130F; ");
	this->resize(1920/2, 1080/2);
	this->setWindowFlags(Qt::FramelessWindowHint);
	//this->setAttribute(Qt::WA_TranslucentBackground);
	
	// Menu stuffs
	
	_settingsWidget = new SettingsWidget(this);
	_settingsWidget->setGeometry(this->width(), 40, 300, this->height() - 40);
	_settingsWidget->raise();

	//


	_minimizeButton = new QPushButton(NULL, this);
	_maximizeButton = new QPushButton(NULL, this);
	_closeButton = new QPushButton(NULL, this);
	_menuButton = new QPushButton(NULL, this);

	connect(_menuButton, &QPushButton::clicked, this, &PomodoroWindow::toggleSettings);

	assetsLoader();

	designWindow();

	QLabel *PomodoroTitle = new QLabel("Pomodoro", this);
	PomodoroTitle->setStyleSheet("color: white;");
	PomodoroTitle->setFont(_PlaywriteFont);

	QWidget *titleBarContainer = new QWidget(this);
	// debugger
	//titleBarContainer->setStyleSheet("border: 1px solid red;");
	titleBarContainer->setFixedHeight(40);

	QGridLayout *titleBarLayout = new QGridLayout(titleBarContainer);
	titleBarLayout->setContentsMargins(0, 0, 0, 0);
	titleBarLayout->addWidget(PomodoroTitle, 0, 1, Qt::AlignCenter);
	// debug QGridLay

	QHBoxLayout *buttonsLayout = new QHBoxLayout();
	buttonsLayout->addWidget(_minimizeButton);
	buttonsLayout->addWidget(_maximizeButton);
	buttonsLayout->addWidget(_closeButton);
	buttonsLayout->addWidget(_menuButton);

	titleBarLayout->addLayout(buttonsLayout, 0, 2, Qt::AlignRight | Qt::AlignVCenter);

	titleBarLayout->setColumnStretch(0, 1);
	titleBarLayout->setColumnStretch(1, 0);
	titleBarLayout->setColumnStretch(2, 1);


	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addWidget(titleBarContainer);
	mainLayout->addStretch();


	
	TimerWidget *timer = new TimerWidget(this);
	//timer->setStyleSheet("border: 1px solid brown;");

	_settingsWidget->setTimerContext(timer);

	mainLayout->addWidget(timer);
	mainLayout->addStretch();

	
	QObject::connect(_closeButton, &QPushButton::clicked, this, &QApplication::quit);
	QObject::connect(_maximizeButton, &QPushButton::clicked, this, [this](){this->isMaximized() ? this->showNormal() : this->showMaximized();});
	QObject::connect(_minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
}

void	PomodoroWindow::execute()
{

}

void	PomodoroWindow::assetsLoader()
{
	int	PlaywriteFontId = QFontDatabase::addApplicationFont(":/PlaywriteARFont");
	if (PlaywriteFontId == -1)
		std::cerr << "font not loaded" << std::endl;

	_PlaywriteFont = QFont(QFontDatabase::applicationFontFamilies(PlaywriteFontId).at(0), 16);


	_closeButton->setIcon(QIcon(":/closeButton"));
	_maximizeButton->setIcon(QIcon(":/maximizeButton"));
	_minimizeButton->setIcon(QIcon(":/minimizeButton"));
	_menuButton->setIcon(QIcon(":/menuButton"));
}

void	PomodoroWindow::changeEvent(QEvent *event)
{
	// changing icon while maximizing reverse maximizing!
	 if (event->type() == QEvent::WindowStateChange) {
		(this->isMaximized()	? _maximizeButton->setIcon(QIcon(":/maximizeReverseButton"))
								: _maximizeButton->setIcon(QIcon(":/maximizeButton"))
		);
	}

	QWidget::changeEvent(event);
}

void	PomodoroWindow::designWindow()
{
	_closeButton->setFixedSize(45, 30);
	_closeButton->setStyleSheet(
			//"border: 1px solid red;"
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: red; color: #FFFFFF; }"
			"QPushButton:pressed { background: #c9383b; }"
		);

	_maximizeButton->setFixedSize(45, 30);
	_maximizeButton->setStyleSheet(
			//"border: 1px solid red;"
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);

	_minimizeButton->setFixedSize(45, 30);
	_minimizeButton->setStyleSheet(
			//"border: 1px solid red;"
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);

	_menuButton->setFixedSize(45, 30);
	_menuButton->setStyleSheet(
			//"border: 1px solid red;"
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);
}
void PomodoroWindow::toggleSettings()
{
    int menuWidth = 300;
    int titleBarHeight = 40;
    int currentHeight = this->height() - titleBarHeight;
    
	_settingsWidget->raise();

    QPropertyAnimation *animation = new QPropertyAnimation(_settingsWidget, "geometry");
    animation->setDuration(300); 
    animation->setEasingCurve(QEasingCurve::InBack); 

    if (_isSettingsOpen)
        animation->setEndValue(QRect(this->width(), titleBarHeight, menuWidth, currentHeight));
    else
        animation->setEndValue(QRect(this->width() - menuWidth, titleBarHeight, menuWidth, currentHeight));

    animation->start(QAbstractAnimation::DeleteWhenStopped);
    _isSettingsOpen = !_isSettingsOpen;
}

void PomodoroWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (_settingsWidget) {
        int menuWidth = 300;
        int titleBarHeight = 40; 
        int newHeight = this->height() - titleBarHeight;

        if (_isSettingsOpen)
            _settingsWidget->setGeometry(this->width() - menuWidth, titleBarHeight, menuWidth, newHeight);
        else
            _settingsWidget->setGeometry(this->width(), titleBarHeight, menuWidth, newHeight);
    }
}
