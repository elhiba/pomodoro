#ifndef POMODORO_WINDOW_HPP
#define POMODORO_WINDOW_HPP

#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

#include "SettingsWidget.hpp"

class SettingsWidget;

class PomodoroWindow : public QWidget
{
	private:
		QPushButton		*_minimizeButton;
		QPushButton		*_maximizeButton;
		QPushButton		*_closeButton;
		QPushButton		*_menuButton;

		QFont			_PlaywriteFont;

		SettingsWidget	*_settingsWidget;
		bool			_isSettingsOpen = false;

	    bool _autoStartTimer = false;
	    bool _autoStartMusic = true;

	private slots:
		void	toggleSettings();

	public:
		PomodoroWindow();

		void	assetsLoader();
		void	execute();

	protected:
		void	changeEvent(QEvent *event) override;
		void	designWindow();
		void	mousePressEvent();
		void    resizeEvent(QResizeEvent *event) override;
};

#endif
