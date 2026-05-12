#include "PomodoroWindow.hpp"
#include "TimerWidget.hpp"

#include <QFontDatabase>
#include <QFont>
#include <iostream>

PomodoroWindow::PomodoroWindow()
{
	this->setStyleSheet("background-color: #1c1e2b; ");
	this->resize(1920/2, 1080/2);
	this->setWindowFlags(Qt::FramelessWindowHint);
	//this->setAttribute(Qt::WA_TranslucentBackground);


	_minimizeButton = new QPushButton(NULL, this);
	_maximizeButton = new QPushButton(NULL, this);
	_closeButton = new QPushButton(NULL, this);

	assetsLoader();

	designWindow();

	QLabel *PomodoroTitle = new QLabel("Pomodoro", this);
	PomodoroTitle->setFont(_PlaywriteFont);

	QWidget *titleBarContainer = new QWidget(this);
	// debugger
	titleBarContainer->setStyleSheet("border: 1px solid red;");
	titleBarContainer->setFixedHeight(40);

	QGridLayout *titleBarLayout = new QGridLayout(titleBarContainer);
	titleBarLayout->setContentsMargins(0, 0, 0, 0);
	titleBarLayout->addWidget(PomodoroTitle, 0, 1, Qt::AlignCenter);
	// debug QGridLay

	QHBoxLayout *buttonsLayout = new QHBoxLayout();
	buttonsLayout->addWidget(_minimizeButton);
	buttonsLayout->addWidget(_maximizeButton);
	buttonsLayout->addWidget(_closeButton);

	titleBarLayout->addLayout(buttonsLayout, 0, 2, Qt::AlignRight | Qt::AlignVCenter);

	titleBarLayout->setColumnStretch(0, 1);
	titleBarLayout->setColumnStretch(1, 0);
	titleBarLayout->setColumnStretch(2, 1);


	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addWidget(titleBarContainer);
	mainLayout->addStretch();


	
	QWidget	*timer = new TimerWidget(this);
	timer->setStyleSheet("border: 1px solid brown;");

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
	int	PlaywriteFontId = QFontDatabase::addApplicationFont(":assets/fonts/PlaywriteAR.ttf");
	if (PlaywriteFontId == -1)
		std::cerr << "font not loaded" << std::endl;

	_PlaywriteFont = QFont(QFontDatabase::applicationFontFamilies(PlaywriteFontId).at(0), 16);


	_closeButton->setIcon(QIcon(":/assets/icons/close.svg"));
	_maximizeButton->setIcon(QIcon(":/assets/icons/maximize.svg"));
	_minimizeButton->setIcon(QIcon(":/assets/icons/minimize.svg"));
}

void	PomodoroWindow::changeEvent(QEvent *event)
{
	// changing icon while maximizing reverse maximizing!
	 if (event->type() == QEvent::WindowStateChange) {
		(this->isMaximized()	? _maximizeButton->setIcon(QIcon(":/assets/icons/maximizeReverse.svg"))
								: _maximizeButton->setIcon(QIcon(":/assets/icons/maximize.svg"))
		);
	}

	QWidget::changeEvent(event);
}

void	PomodoroWindow::designWindow()
{
	_closeButton->setFixedSize(45, 30);
	_closeButton->setStyleSheet(
			"border: 1px solid red;"
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: red; color: #FFFFFF; }"
			"QPushButton:pressed { background: #c9383b; }"
		);

	_maximizeButton->setFixedSize(45, 30);
	_maximizeButton->setStyleSheet(
			"border: 1px solid red;"
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);

	_minimizeButton->setFixedSize(45, 30);
	_minimizeButton->setStyleSheet(
			"border: 1px solid red;"
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);
}
