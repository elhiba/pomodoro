#include "UpdateChecker.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

const char *const	UpdateChecker::LatestReleaseUrl =
	"https://api.github.com/repos/elhiba/pomodoro/releases/latest";

const char *const	UpdateChecker::ReleasesPageUrl =
	"https://github.com/elhiba/pomodoro/releases/latest";

UpdateChecker::UpdateChecker(QObject *parent)
	: QObject(parent)
{
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
			return QStringLiteral("Checking…");
		case UpToDate:
			return QStringLiteral("Up to date (%1)").arg(currentVersion());
		case UpdateAvailable:
			return QStringLiteral("Version %1 is available").arg(_latestVersion);
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
	return _status == Checking;
}

bool	UpdateChecker::updateAvailable() const
{
	return _status == UpdateAvailable;
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
	if (_reply)
		return;

	QNetworkRequest	request((QUrl(QLatin1String(LatestReleaseUrl))));

	request.setRawHeader("Accept", "application/vnd.github+json");
	request.setRawHeader("User-Agent",
		(QCoreApplication::applicationName() + QLatin1Char('/') + currentVersion()).toLatin1());

	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	setStatus(Checking);

	_reply = _network.get(request);

	connect(_reply, &QNetworkReply::finished, this, &UpdateChecker::onFinished);

	// A request that never answers must not leave the button spinning for ever.
	QTimer::singleShot(TimeoutMs, _reply, [this]()
	{
		if (_reply)
			_reply->abort();
	});
}

void	UpdateChecker::openDownloadPage()
{
	QString	url = _releaseUrl.isEmpty() ? QLatin1String(ReleasesPageUrl) : _releaseUrl;

	QDesktopServices::openUrl(QUrl(url));
}

void	UpdateChecker::onFinished()
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
	_releaseUrl = object.value(QStringLiteral("html_url")).toString();

	if (compareVersions(_latestVersion, normalise(currentVersion())) > 0)
		setStatus(UpdateAvailable);
	else
		setStatus(UpToDate);
}

void	UpdateChecker::setStatus(Status status, const QString &errorText)
{
	if (_status == status && _errorText == errorText)
		return;

	_status = status;
	_errorText = errorText;

	emit statusChanged();
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
