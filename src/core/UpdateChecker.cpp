#include "UpdateChecker.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

const char *const	UpdateChecker::LatestReleaseUrl =
	"https://api.github.com/repos/elhiba/pomodoro/releases/latest";

namespace
{
	// Where an update is saved before it is installed. Emptied at start-up, since the
	// last one has done its job by the time the app it installed is running.
	QString	updateTempDirectory()
	{
		return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
			+ QStringLiteral("/pomodoro-update");
	}

#ifdef Q_OS_WIN
	// Inno Setup's uninstall entry: the AppId from packaging/windows/pomodoro.iss plus
	// "_is1". Its InstallLocation is where the installer put the app, which is how a copy
	// tells whether it is the installed one or an unzipped portable folder. It lives under
	// HKCU for the default per-user install and under HKLM when the installer's dialog was
	// used to install for everyone into Program Files.
	const char *const	UninstallSubkey =
		"\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
		"{9F1C4A2E-7B83-4D56-9C21-0E5A8D3B6F14}_is1";

	// Set by detectInstallKind() when the install found was the all-users one.
	bool	installedForAllUsers = false;

	// Unpacks the portable zip over the folder the app runs from, once the app has
	// exited, and starts it again. robocopy rather than Copy-Item: it merges into the
	// existing folders instead of nesting them, and retries a file Windows still holds
	// open for a moment after the process that used it has gone.
	const char *const	PortableUpdateScript = R"ps(
param([int]$ProcessId, [string]$Zip, [string]$Target, [int]$Relaunch)

Wait-Process -Id $ProcessId -Timeout 60 -ErrorAction SilentlyContinue

$staging = Join-Path (Split-Path -Parent $Zip) 'unpacked'

if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }

try {
	Expand-Archive -LiteralPath $Zip -DestinationPath $staging -Force -ErrorAction Stop

	# The release zip holds a pomodoro\ folder; accept one packed without it too.
	$source = $staging
	$inner = Join-Path $staging 'pomodoro'
	if (Test-Path -LiteralPath (Join-Path $inner 'pomodoro.exe')) { $source = $inner }

	robocopy $source $Target /E /R:30 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
} catch {}

if ($Relaunch -eq 1) {
	Start-Process -FilePath (Join-Path $Target 'pomodoro.exe') -WorkingDirectory $Target
}
)ps";

	// Whether this process can write into a folder. QFileInfo::isWritable() does not look
	// at NTFS permissions, so the only honest answer is to try.
	bool	canWriteInto(const QString &directory)
	{
		QTemporaryFile	probe(directory + QStringLiteral("/.pomodoro-write-test-XXXXXX"));

		return probe.open();
	}
#endif

#ifdef Q_OS_MACOS
	// Swaps the .app bundle for the one on the new disk image once the app has exited,
	// putting the old one back if the copy fails, and starts it again. $1 is the pid to
	// wait for, $2 the dmg, $3 the bundle to replace, $4 whether to relaunch.
	const char *const	MacUpdateScript = R"sh(
pid="$1"; dmg="$2"; app="$3"; relaunch="$4"

while kill -0 "$pid" 2>/dev/null; do sleep 0.2; done

mnt=$(mktemp -d "${TMPDIR:-/tmp}/pomodoro-update.XXXXXX") || exit 1

if hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$mnt" "$dmg" >/dev/null; then
	new=$(find "$mnt" -maxdepth 1 -name '*.app' 2>/dev/null | head -n 1)

	if [ -n "$new" ] && rm -rf "$app.old" && mv "$app" "$app.old"; then
		if ditto "$new" "$app"; then
			rm -rf "$app.old"
		else
			rm -rf "$app"
			mv "$app.old" "$app"
		fi
	fi

	hdiutil detach "$mnt" -quiet
fi

rmdir "$mnt" 2>/dev/null
rm -f "$dmg"
xattr -dr com.apple.quarantine "$app" 2>/dev/null

if [ "$relaunch" = 1 ]; then open "$app"; fi
)sh";

	// The .app bundle this copy runs from, or nothing when it is not in one.
	QString	macBundlePath()
	{
		QString	bundle = QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).canonicalPath();

		return bundle.endsWith(QLatin1String(".app")) ? bundle : QString();
	}
#endif
}

UpdateChecker::UpdateChecker(QObject *parent)
	: QObject(parent),
	_installKind(detectInstallKind())
{
	QDir(updateTempDirectory()).removeRecursively();

	connect(qApp, &QCoreApplication::aboutToQuit, this, &UpdateChecker::onAboutToQuit);
}

UpdateChecker::~UpdateChecker()
{
	// A half-finished download is worthless; do not leave it behind.
	if (_download)
	{
		_download->abort();
		_downloadFile.remove();
	}
}

UpdateChecker::Status	UpdateChecker::status() const
{
	return _status;
}

QString	UpdateChecker::statusText() const
{
	switch (_status)
	{
		case Checking:
			return QStringLiteral("Checking for updates…");
		case UpToDate:
			return QStringLiteral("Up to date (%1)").arg(currentVersion());
		case UpdateAvailable:
			if (_installKind == NotInstallable)
				return QStringLiteral("Version %1 is available (you have %2). This build cannot replace itself; "
					"update it the way you installed it")
					.arg(_latestVersion, currentVersion());

			return QStringLiteral("Version %1 is available (you have %2)")
				.arg(_latestVersion, currentVersion());
		case Downloading:
			return QStringLiteral("Downloading version %1… %2%")
				.arg(_latestVersion)
				.arg(qRound(_downloadProgress * 100));
		case ReadyToInstall:
			return QStringLiteral("Version %1 is downloaded and installs as soon as the timer is stopped")
				.arg(_latestVersion);
		case Installing:
			return QStringLiteral("Restarting into version %1…").arg(_latestVersion);
		case Failed:
			return _errorText.isEmpty()
				? QStringLiteral("Could not check for updates")
				: _errorText;
		case Idle:
		default:
			return QStringLiteral("Version %1").arg(currentVersion());
	}
}

bool	UpdateChecker::busy() const
{
	return _status == Checking || _status == Downloading || _status == Installing;
}

// Stays true through a download and after one fails, so the offer to update does not
// vanish the moment something goes wrong -- trying again is the obvious next step.
bool	UpdateChecker::updateAvailable() const
{
	return !_latestVersion.isEmpty()
		&& compareVersions(_latestVersion, normalise(currentVersion())) > 0;
}

bool	UpdateChecker::canInstall() const
{
	return _installKind != NotInstallable && !_assetUrl.isEmpty() && !_assetSha256.isEmpty();
}

bool	UpdateChecker::readyToInstall() const
{
	return _status == ReadyToInstall;
}

qreal	UpdateChecker::downloadProgress() const
{
	return _downloadProgress;
}

QString	UpdateChecker::latestVersion() const
{
	return _latestVersion;
}

QString	UpdateChecker::currentVersion() const
{
	return QCoreApplication::applicationVersion();
}

void	UpdateChecker::check()
{
	// Once an update is downloaded there is nothing newer worth asking about until it is in.
	if (_reply || _download || _status == ReadyToInstall || _status == Installing)
		return;

	QNetworkRequest	request((QUrl(QLatin1String(LatestReleaseUrl))));

	request.setRawHeader("Accept", "application/vnd.github+json");
	request.setRawHeader("User-Agent",
		(QCoreApplication::applicationName() + QLatin1Char('/') + currentVersion()).toLatin1());

	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	setStatus(Checking);

	_reply = _network.get(request);

	connect(_reply, &QNetworkReply::finished, this, &UpdateChecker::onCheckFinished);

	// A request that never answers must not leave the button spinning for ever.
	QTimer::singleShot(CheckTimeoutMs, _reply, [this]()
	{
		if (_reply)
			_reply->abort();
	});
}

void	UpdateChecker::installUpdate()
{
	if (_download || _status == ReadyToInstall || _status == Installing
		|| !updateAvailable() || !canInstall())
		return;

	QString	path = downloadPath();

	QDir().mkpath(QFileInfo(path).absolutePath());

	_downloadFile.setFileName(path);

	if (!_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		setStatus(Failed, QStringLiteral("Could not save the update: %1").arg(_downloadFile.errorString()));
		return;
	}

	_downloadHash.reset();
	setDownloadProgress(0.0);

	QNetworkRequest	request((QUrl(_assetUrl)));

	request.setRawHeader("Accept", "application/octet-stream");
	request.setRawHeader("User-Agent",
		(QCoreApplication::applicationName() + QLatin1Char('/') + currentVersion()).toLatin1());

	// GitHub answers with a redirect to its download host; following it is fine as long
	// as it never drops from HTTPS to HTTP.
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	setStatus(Downloading);

	_download = _network.get(request);

	connect(_download, &QNetworkReply::readyRead, this, &UpdateChecker::onDownloadReadyRead);
	connect(_download, &QNetworkReply::downloadProgress, this, &UpdateChecker::onDownloadProgress);
	connect(_download, &QNetworkReply::finished, this, &UpdateChecker::onDownloadFinished);
}

void	UpdateChecker::applyUpdate()
{
	if (_status != ReadyToInstall || !launch(_downloadedPath, true))
		return;

	setStatus(Installing);
	emit quitting();

	// A moment for the status line to be seen before the window goes.
	QTimer::singleShot(600, qApp, &QCoreApplication::quit);
}

// Quitting with a verified update waiting puts it in place on the way out, without
// starting the app again: the user asked for it to close.
void	UpdateChecker::onAboutToQuit()
{
	if (_status == ReadyToInstall)
		launch(_downloadedPath, false);
}

void	UpdateChecker::onCheckFinished()
{
	if (!_reply)
		return;

	QNetworkReply	*reply = _reply;

	_reply = nullptr;
	reply->deleteLater();

	int	httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	if (reply->error() != QNetworkReply::NoError)
	{
		if (reply->error() == QNetworkReply::OperationCanceledError)
			setStatus(Failed, QStringLiteral("The update check timed out"));
		else if (httpStatus == 404)
		{
			// GitHub answers 404 both for a repository it will not show anonymously and
			// for one that has published no release yet, and cannot tell them apart
			// without credentials -- which a distributed app has no business carrying.
			setStatus(Failed, QStringLiteral("No published release found"));
		}
		else if (httpStatus == 403)
			setStatus(Failed, QStringLiteral("GitHub is rate limiting, try again later"));
		else if (httpStatus > 0)
			setStatus(Failed, QStringLiteral("GitHub answered %1").arg(httpStatus));
		else
			setStatus(Failed, QStringLiteral("Could not reach GitHub"));

		return;
	}

	QJsonParseError	error;
	QJsonDocument	document = QJsonDocument::fromJson(reply->readAll(), &error);

	if (error.error != QJsonParseError::NoError || !document.isObject())
	{
		setStatus(Failed, QStringLiteral("GitHub sent something unreadable"));
		return;
	}

	QJsonObject	object = document.object();
	QString		tag = object.value(QStringLiteral("tag_name")).toString();

	if (tag.isEmpty())
	{
		setStatus(Failed, QStringLiteral("No release has been published yet"));
		return;
	}

	_latestVersion = normalise(tag);

	pickAsset(object.value(QStringLiteral("assets")).toArray());

	setStatus(updateAvailable() ? UpdateAvailable : UpToDate);
}

void	UpdateChecker::onDownloadReadyRead()
{
	if (!_download)
		return;

	QByteArray	chunk = _download->readAll();

	_downloadHash.addData(chunk);

	if (_downloadFile.write(chunk) != chunk.size())
		failDownload(QStringLiteral("Could not save the update: %1").arg(_downloadFile.errorString()));
}

void	UpdateChecker::onDownloadProgress(qint64 received, qint64 total)
{
	if (total > 0)
		setDownloadProgress(static_cast<qreal>(received) / static_cast<qreal>(total));
}

void	UpdateChecker::onDownloadFinished()
{
	if (!_download)
		return;

	QNetworkReply	*reply = _download;

	// Anything still buffered belongs in the file and the hash before it is judged.
	onDownloadReadyRead();

	if (!_download)
		return;

	_download = nullptr;
	reply->deleteLater();

	_downloadFile.close();

	if (reply->error() != QNetworkReply::NoError)
	{
		_downloadFile.remove();
		setStatus(Failed, QStringLiteral("The download failed: %1").arg(reply->errorString()));
		return;
	}

	QString	actual = QString::fromLatin1(_downloadHash.result().toHex());

	// The one check that stands between the network and running a program. A mismatch
	// means the file is not what GitHub says it published, whatever the reason.
	if (actual.compare(_assetSha256, Qt::CaseInsensitive) != 0)
	{
		_downloadFile.remove();
		setStatus(Failed, QStringLiteral("The download did not match its published checksum, so it was discarded"));
		return;
	}

	setDownloadProgress(1.0);

	_downloadedPath = _downloadFile.fileName();

	// Main.qml installs it the moment the timer is idle, or it goes in when the app quits.
	setStatus(ReadyToInstall);
}

void	UpdateChecker::setStatus(Status status, const QString &errorText)
{
	if (_status == status && _errorText == errorText)
		return;

	_status = status;
	_errorText = errorText;

	emit statusChanged();
}

void	UpdateChecker::setDownloadProgress(qreal progress)
{
	if (qFuzzyCompare(_downloadProgress, progress))
		return;

	_downloadProgress = progress;

	emit downloadProgressChanged();

	// The percentage is part of the status line.
	if (_status == Downloading)
		emit statusChanged();
}

void	UpdateChecker::failDownload(const QString &reason)
{
	QNetworkReply	*reply = _download;

	_download = nullptr;

	if (reply)
	{
		reply->disconnect(this);
		reply->abort();
		reply->deleteLater();
	}

	_downloadFile.close();
	_downloadFile.remove();

	setStatus(Failed, reason);
}

// The release asset this copy would update from, and the digest to hold it to. Matched
// by the suffix release.yml gives each platform's file.
void	UpdateChecker::pickAsset(const QJsonArray &assets)
{
	_assetName.clear();
	_assetUrl.clear();
	_assetSha256.clear();

	QString	suffix;

	switch (_installKind)
	{
		case WindowsInstaller:
			suffix = QStringLiteral("-windows-x64-setup.exe");
			break;
		case WindowsPortable:
			suffix = QStringLiteral("-windows-x64.zip");
			break;
		case LinuxAppImage:
			suffix = QStringLiteral("-x86_64.AppImage");
			break;
		case MacDiskImage:
			suffix = QStringLiteral("-macos.dmg");
			break;
		case NotInstallable:
		default:
			return;
	}

	for (const QJsonValue &value : assets)
	{
		QJsonObject	asset = value.toObject();
		QString		name = asset.value(QStringLiteral("name")).toString();

		if (!name.endsWith(suffix, Qt::CaseInsensitive))
			continue;

		// "sha256:<hex>". An asset without one cannot be verified, and is not installed.
		QString	digest = asset.value(QStringLiteral("digest")).toString();

		if (!digest.startsWith(QLatin1String("sha256:")))
			return;

		_assetName = name;
		_assetUrl = asset.value(QStringLiteral("browser_download_url")).toString();
		_assetSha256 = digest.mid(7);
		return;
	}
}

QString	UpdateChecker::downloadPath() const
{
	// Beside the AppImage it is going to replace, so the final step is a rename within one
	// filesystem rather than a copy that could be interrupted halfway.
	if (_installKind == LinuxAppImage)
	{
		QFileInfo	current(qEnvironmentVariable("APPIMAGE"));
		QFileInfo	directory(current.absolutePath());

		if (directory.isWritable())
			return current.absolutePath() + QStringLiteral("/.") + _assetName + QStringLiteral(".part");
	}

	return updateTempDirectory() + QLatin1Char('/') + _assetName;
}

// Puts the verified download in place. With relaunch the new version is started once this
// process has gone; without it (the app is quitting) it is simply there next time.
bool	UpdateChecker::launch(const QString &path, bool relaunch)
{
	switch (_installKind)
	{
		case WindowsInstaller:
		{
			// /SILENT shows only a progress bar; /RELAUNCH=1 is read by the [Run] entry in
			// pomodoro.iss to start the new version when it is done. /CLOSEAPPLICATIONS is
			// the backstop in case this process has not exited by the time files are copied.
			QStringList	arguments = {
				QStringLiteral("/SILENT"),
				QStringLiteral("/SUPPRESSMSGBOXES"),
				QStringLiteral("/NORESTART"),
				QStringLiteral("/CLOSEAPPLICATIONS")
			};

			if (relaunch)
				arguments.append(QStringLiteral("/RELAUNCH=1"));

			// Updating an install in Program Files has to go back there, which takes
			// administrator rights: Windows asks once, as it did the first time.
#ifdef Q_OS_WIN
			if (installedForAllUsers)
				arguments.append(QStringLiteral("/ALLUSERS"));
#endif

			if (!QProcess::startDetached(path, arguments))
			{
				setStatus(Failed, QStringLiteral("Could not start the installer"));
				return false;
			}

			return true;
		}

		case WindowsPortable:
		{
#ifdef Q_OS_WIN
			QString	scriptPath = updateTempDirectory() + QStringLiteral("/apply-update.ps1");
			QFile	script(scriptPath);

			if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)
				|| script.write(PortableUpdateScript) < 0)
			{
				setStatus(Failed, QStringLiteral("Could not prepare the update: %1").arg(script.errorString()));
				return false;
			}

			script.close();

			QProcess	process;

			process.setProgram(QStringLiteral("powershell.exe"));
			process.setArguments({
				QStringLiteral("-NoProfile"),
				QStringLiteral("-NonInteractive"),
				QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
				QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
				QStringLiteral("-File"), QDir::toNativeSeparators(scriptPath),
				QStringLiteral("-ProcessId"), QString::number(QCoreApplication::applicationPid()),
				QStringLiteral("-Zip"), QDir::toNativeSeparators(path),
				QStringLiteral("-Target"), QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),
				QStringLiteral("-Relaunch"), relaunch ? QStringLiteral("1") : QStringLiteral("0")
			});

			// No console window flashing up while it works.
			process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments)
			{
				arguments->flags |= CREATE_NO_WINDOW;
			});

			if (!process.startDetached())
			{
				setStatus(Failed, QStringLiteral("Could not start the updater"));
				return false;
			}

			return true;
#else
			return false;
#endif
		}

		case LinuxAppImage:
		{
			QString	target = qEnvironmentVariable("APPIMAGE");

			QFile::setPermissions(path, QFile::permissions(path)
				| QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther
				| QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);

			// Replacing a running AppImage is safe on Linux: this process keeps the old
			// file open until it exits, and the name now points at the new one.
			bool	sameDirectory = QFileInfo(path).absolutePath() == QFileInfo(target).absolutePath();
			QString	staged = target + QStringLiteral(".new");

			if (!sameDirectory)
			{
				QFile::remove(staged);

				if (!QFile::copy(path, staged))
				{
					setStatus(Failed, QStringLiteral("Could not write next to %1").arg(target));
					return false;
				}

				QFile::remove(path);
			}
			else
				staged = path;

			QFile::remove(target);

			if (!QFile::rename(staged, target))
			{
				setStatus(Failed, QStringLiteral("Could not replace %1").arg(target));
				return false;
			}

			// Started a moment after this process has gone, so the new copy does not find
			// the single-instance lock still held and simply hand over to the old one.
			if (relaunch)
				QProcess::startDetached(QStringLiteral("/bin/sh"),
					{ QStringLiteral("-c"), QStringLiteral("sleep 1; exec \"$0\""), target });

			return true;
		}

		case MacDiskImage:
		{
#ifdef Q_OS_MACOS
			// $0 names the script; the rest are its arguments.
			bool	started = QProcess::startDetached(QStringLiteral("/bin/sh"), {
				QStringLiteral("-c"), QLatin1String(MacUpdateScript),
				QStringLiteral("pomodoro-update"),
				QString::number(QCoreApplication::applicationPid()),
				path,
				macBundlePath(),
				relaunch ? QStringLiteral("1") : QStringLiteral("0")
			});

			if (!started)
			{
				setStatus(Failed, QStringLiteral("Could not start the updater"));
				return false;
			}

			return true;
#else
			return false;
#endif
		}

		case NotInstallable:
		default:
			return false;
	}
}

UpdateChecker::InstallKind	UpdateChecker::detectInstallKind()
{
#if defined(Q_OS_WIN)
	QString	running = QDir(QCoreApplication::applicationDirPath()).canonicalPath();

	for (const char *hive : { "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE" })
	{
		QSettings	uninstall(QLatin1String(hive) + QLatin1String(UninstallSubkey), QSettings::NativeFormat);
		QString		location = uninstall.value(QStringLiteral("InstallLocation")).toString();

		if (location.isEmpty())
			continue;

		QString	installed = QDir(location).canonicalPath();

		if (!installed.isEmpty() && installed.compare(running, Qt::CaseInsensitive) == 0)
		{
			installedForAllUsers = qstrcmp(hive, "HKEY_LOCAL_MACHINE") == 0;
			return WindowsInstaller;
		}
	}

	// Not the installed copy, so the unzipped portable folder -- unless it is a build
	// tree, which is a developer's to manage and not something to unzip a release over.
	if (QFileInfo::exists(running + QStringLiteral("/CMakeCache.txt")))
		return NotInstallable;

	return canWriteInto(running) ? WindowsPortable : NotInstallable;
#elif defined(Q_OS_MACOS)
	QString	bundle = macBundlePath();

	if (bundle.isEmpty())
		return NotInstallable;

	QString	parent = QFileInfo(bundle).absolutePath();

	// Run straight off the disk image, or out of a build tree: nothing to replace.
	if (!QFileInfo(parent).isWritable() || QFileInfo::exists(parent + QStringLiteral("/CMakeCache.txt")))
		return NotInstallable;

	return MacDiskImage;
#else
	// Set by the AppImage runtime to the file that was launched.
	QString	appImage = qEnvironmentVariable("APPIMAGE");

	if (!appImage.isEmpty() && QFileInfo::exists(appImage))
		return LinuxAppImage;

	return NotInstallable;
#endif
}

QString	UpdateChecker::normalise(const QString &tag)
{
	QString	text = tag.trimmed();

	// Releases are tagged "v1.2.3"; the version inside the app is not.
	if (text.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
		text.remove(0, 1);

	return text;
}

int	UpdateChecker::compareVersions(const QString &left, const QString &right)
{
	const QStringList	leftParts = left.split(QLatin1Char('.'));
	const QStringList	rightParts = right.split(QLatin1Char('.'));

	for (int index = 0; index < qMax(leftParts.size(), rightParts.size()); index++)
	{
		// A missing component counts as zero, so "1.2" and "1.2.0" are the same version.
		int	leftValue = index < leftParts.size() ? leftParts.at(index).toInt() : 0;
		int	rightValue = index < rightParts.size() ? rightParts.at(index).toInt() : 0;

		if (leftValue != rightValue)
			return leftValue < rightValue ? -1 : 1;
	}

	return 0;
}
