#include "PomodoroWindow.hpp"

PomodoroWindow::PomodoroWindow()
{
	this->setStyleSheet("background-color: #1c1e2b; ");
	this->resize(1920/2, 1080/2);
	this->setWindowFlags(Qt::FramelessWindowHint);


	_minimizeButton = new QPushButton(NULL, this);
	_maximizeButton = new QPushButton(NULL, this);
	_closeButton = new QPushButton(NULL, this);

	designWindow();
	
	QObject::connect(_closeButton, &QPushButton::clicked, this, &QApplication::quit);
	QObject::connect(_maximizeButton, &QPushButton::clicked, this, [this](){this->isMaximized() ? this->showNormal() : this->showMaximized();});
	QObject::connect(_minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
}

void	PomodoroWindow::resizeEvent(QResizeEvent *event)
{
	_closeButton->move(this->width() - _closeButton->width(), 0);
	_maximizeButton->move(this->width() - (_maximizeButton->width() + _closeButton->width()), 0);
	_minimizeButton->move(this->width() - (_minimizeButton->width() + _maximizeButton->width() + _closeButton->width()), 0);

	// changing icon while maximizing reverse maximizing!
	(this->isMaximized() ? _maximizeButton->setIcon(QIcon(":/assets/icons/maximizeReverse.svg")) : _maximizeButton->setIcon(QIcon(":/assets/icons/maximize.svg")));

	QWidget::resizeEvent(event);
}

void	PomodoroWindow::designWindow()
{
	_closeButton->setIcon(QIcon(":/assets/icons/close.svg"));
	_closeButton->resize(45, 30);
	_closeButton->setStyleSheet(
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: red; color: #FFFFFF; }"
			"QPushButton:pressed { background: #c9383b; }"
		);

	_maximizeButton->setIcon(QIcon(":/assets/icons/maximize.svg"));
	_maximizeButton->resize(45, 30);
	_maximizeButton->setStyleSheet(
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);

	_minimizeButton->setIcon(QIcon(":/assets/icons/minimize.svg"));
	_minimizeButton->resize(45, 30);
	_minimizeButton->setStyleSheet(
			"QPushButton { color: #FFFFFF; background: transparent; border: none; font-size: 14px; }"
			"QPushButton:hover { background: grey; }"
			"QPushButton:pressed { background: #FFFFFF; }"
		);

}
