#include "PomodoroWindow.hpp"
#include "TimerWidget.hpp"

PomodoroWindow::PomodoroWindow()
{
	this->setStyleSheet("background-color: #1c1e2b; ");
	this->resize(1920/2, 1080/2);
	this->setWindowFlags(Qt::FramelessWindowHint);


	_minimizeButton = new QPushButton(NULL, this);
	_maximizeButton = new QPushButton(NULL, this);
	_closeButton = new QPushButton(NULL, this);

	QLabel *PomodoroTitle = new QLabel("Pomodoro", this);

	QWidget *titleBarContainer = new QWidget(this);
	titleBarContainer->setFixedHeight(40);

	QGridLayout *titleBarLayout = new QGridLayout(titleBarContainer);
	titleBarLayout->setContentsMargins(0, 0, 0, 0);
	titleBarLayout->addWidget(PomodoroTitle, 0, 1, Qt::AlignCenter);

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

	mainLayout->addWidget(timer);
	mainLayout->addStretch();

	designWindow();
	
	QObject::connect(_closeButton, &QPushButton::clicked, this, &QApplication::quit);
	QObject::connect(_maximizeButton, &QPushButton::clicked, this, [this](){this->isMaximized() ? this->showNormal() : this->showMaximized();});
	QObject::connect(_minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
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
	_closeButton->setIcon(QIcon(":/assets/icons/close.svg"));
	_closeButton->setFixedSize(45, 30);
	_closeButton->setStyleSheet(
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: red; color: #FFFFFF; }"
			"QPushButton:pressed { background: #c9383b; }"
		);

	_maximizeButton->setIcon(QIcon(":/assets/icons/maximize.svg"));
	_maximizeButton->setFixedSize(45, 30);
	_maximizeButton->setStyleSheet(
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);

	_minimizeButton->setIcon(QIcon(":/assets/icons/minimize.svg"));
	_minimizeButton->setFixedSize(45, 30);
	_minimizeButton->setStyleSheet(
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);
}
