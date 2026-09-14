#include "ShellIdentity.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>

#include <windows.h>
#include <objbase.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace
{
	// Reverse-DNS-ish and stable: the shell keys everything off this string, so changing
	// it later orphans the shortcut and the name goes back to "Unknown app".
	const wchar_t *const	AppUserModelId = L"elhiba.pomodoro";

	const char *const	ShortcutName = "Pomodoro.lnk";

	// What an existing shortcut points at, so a stale one can be spotted. An empty
	// string means it could not be read, which is treated the same as pointing
	// somewhere else.
	QString	shortcutTarget(const QString &shortcutPath)
	{
		IShellLinkW	*link = nullptr;

		if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
			IID_IShellLinkW, reinterpret_cast<void **>(&link))) || !link)
		{
			return QString();
		}

		IPersistFile	*file = nullptr;
		QString			target;

		if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file)))
			&& file)
		{
			QString	native = QDir::toNativeSeparators(shortcutPath);

			if (SUCCEEDED(file->Load(reinterpret_cast<const wchar_t *>(native.utf16()), STGM_READ)))
			{
				wchar_t	buffer[MAX_PATH] = {};

				if (SUCCEEDED(link->GetPath(buffer, MAX_PATH, nullptr, 0)))
					target = QString::fromWCharArray(buffer);
			}

			file->Release();
		}

		link->Release();

		return target;
	}

	// Writes the .lnk that the shell resolves the id through. The icon is taken from the
	// executable, which carries it as a resource, so the flyout shows the logo next to
	// the name.
	bool	writeShortcut(const QString &shortcutPath)
	{
		QString	target = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

		IShellLinkW	*link = nullptr;

		HRESULT	hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
			IID_IShellLinkW, reinterpret_cast<void **>(&link));

		if (FAILED(hr) || !link)
			return false;

		link->SetPath(reinterpret_cast<const wchar_t *>(target.utf16()));
		link->SetIconLocation(reinterpret_cast<const wchar_t *>(target.utf16()), 0);
		link->SetDescription(L"A focus timer with a lo-fi stream");

		// The working directory matters for a portable copy: without it the app would
		// inherit whatever directory the shell happened to be in.
		QString	directory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());

		link->SetWorkingDirectory(reinterpret_cast<const wchar_t *>(directory.utf16()));

		// The part that actually matters. A shortcut without this property is just a
		// shortcut; with it, the shell can map the running process back to this entry.
		IPropertyStore	*store = nullptr;

		hr = link->QueryInterface(IID_IPropertyStore, reinterpret_cast<void **>(&store));

		if (SUCCEEDED(hr) && store)
		{
			PROPVARIANT	value;

			if (SUCCEEDED(InitPropVariantFromString(AppUserModelId, &value)))
			{
				store->SetValue(PKEY_AppUserModel_ID, value);
				store->Commit();

				PropVariantClear(&value);
			}

			store->Release();
		}

		IPersistFile	*file = nullptr;

		hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file));

		bool	saved = false;

		if (SUCCEEDED(hr) && file)
		{
			QString	native = QDir::toNativeSeparators(shortcutPath);

			saved = SUCCEEDED(file->Save(reinterpret_cast<const wchar_t *>(native.utf16()), TRUE));

			file->Release();
		}

		link->Release();

		return saved;
	}
}

void	registerShellIdentity()
{
	// Claimed before anything else, so every window and notification this process opens
	// is stamped with it.
	SetCurrentProcessExplicitAppUserModelID(AppUserModelId);

	// On Windows this is the per-user Start menu's Programs folder, which is exactly
	// where the shell looks.
	QString	programs = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);

	if (programs.isEmpty())
		return;

	QString	shortcutPath = programs + QLatin1Char('/') + QLatin1String(ShortcutName);

	QString	target = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

	// Left alone once it points at this copy. Rewriting it on every launch would churn
	// the Start menu for nothing; rewriting it when it points somewhere else is what
	// keeps a moved or updated copy -- the Windows build ships as a portable folder --
	// from leaving a shortcut aimed at an executable that is no longer there.
	if (QFile::exists(shortcutPath))
	{
		if (shortcutTarget(shortcutPath).compare(target, Qt::CaseInsensitive) == 0)
			return;
	}
	else if (!QDir().mkpath(programs))
		return;

	if (!writeShortcut(shortcutPath))
		qWarning("pomodoro: could not create the Start menu entry, the media flyout will "
			"not know the application's name");
}
