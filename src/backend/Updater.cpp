// Pegasus Frontend
// Copyright (C) 2017-2018  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#include "Updater.h"

#include "Log.h"
#include "Paths.h"

#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#endif

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrl>


namespace {
const QString LOG_TAG = QStringLiteral("Updater");
const QString RELEASE_URL = QStringLiteral(
    "https://api.github.com/repos/luxureousproductions-bit/pegasus-frontend/releases/tags/pegasus-x-latest");

// the APK made for this one's processor
QString wanted_abi()
{
    const QString arch = QSysInfo::buildCpuArchitecture();
    if (arch == QLatin1String("arm64"))
        return QStringLiteral("arm64-v8a");
    if (arch == QLatin1String("arm"))
        return QStringLiteral("armeabi-v7a");
    return arch;
}

bool can_install()
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}
} // namespace


Updater::Updater(QObject* parent)
    : QObject(parent)
{}

QVariantMap Updater::info() const
{
    return QVariantMap {
        { QStringLiteral("current"), PEGASUSX_BUILD },
        { QStringLiteral("latest"), m_latest },
        { QStringLiteral("title"), m_title },
        { QStringLiteral("notes"), m_notes },
        { QStringLiteral("size"), m_size },
        { QStringLiteral("received"), m_received },
        { QStringLiteral("error"), m_error },
        { QStringLiteral("canInstall"), can_install() },
    };
}

void Updater::setState(const QString& state, const QString& error)
{
    m_state = state;
    m_error = error;
    emit changed();
}

QString Updater::apkPath() const
{
    return paths::writableCacheDir() + QStringLiteral("/update/pegasus-x.apk");
}

void Updater::check()
{
    if (m_state == QLatin1String("checking") || m_state == QLatin1String("downloading"))
        return;
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    setState(QStringLiteral("checking"));

    QNetworkRequest request{QUrl(RELEASE_URL)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "PegasusX-Updater");
    request.setTransferTimeout(30000);
    QNetworkReply* const reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();
        if (status != 200) {
            setState(QStringLiteral("error"), status == 0 ? tr("No connection.")
                : status == 404 ? tr("No build has been published yet.")
                : status == 403 ? tr("GitHub is busy (too many checks): try again in a while.")
                : tr("GitHub answered %1.").arg(status));
            return;
        }
        // "build: 2105" in the notes; the APK for this processor
        m_notes = release.value(QStringLiteral("body")).toString();
        m_title = release.value(QStringLiteral("name")).toString();
        static const QRegularExpression rx_build(QStringLiteral("build:\\s*(\\d+)"));
        const QRegularExpressionMatch m = rx_build.match(m_notes);
        m_latest = m.hasMatch() ? m.captured(1).toInt() : 0;
        m_url.clear();
        m_size = 0;
        const QString abi = wanted_abi();
        for (const QJsonValue& v : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = v.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (name.endsWith(QLatin1String(".apk")) && name.contains(abi)) {
                m_url = asset.value(QStringLiteral("browser_download_url")).toString();
                m_size = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble());
            }
        }
        Log::info(LOG_TAG, LOGMSG("This is build %1, the newest is %2").arg(PEGASUSX_BUILD).arg(m_latest));
        if (m_latest > PEGASUSX_BUILD && (!m_url.isEmpty() || !can_install()))
            setState(QStringLiteral("available"));
        else
            setState(QStringLiteral("uptodate"));
    });
}

void Updater::download()
{
    if (m_state != QLatin1String("available") || m_url.isEmpty())
        return;
    if (!can_install()) {
        setState(QStringLiteral("error"), tr("Updates install on Android; get this build from the GitHub release."));
        return;
    }
    QDir().mkpath(QFileInfo(apkPath()).absolutePath());
    QFile::remove(apkPath());
    delete m_file;
    m_file = new QFile(apkPath(), this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        setState(QStringLiteral("error"), tr("Couldn't save the download."));
        return;
    }
    m_received = 0;
    setState(QStringLiteral("downloading"));

    QNetworkRequest request{QUrl(m_url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "PegasusX-Updater");
    request.setTransferTimeout(60000);
    QNetworkReply* const reply = m_net->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]{
        if (m_file)
            m_file->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total){
        m_received = received;
        if (total > 0)
            m_size = total;
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool aborted = reply->error() == QNetworkReply::OperationCanceledError;
        if (m_file) {
            m_file->write(reply->readAll());
            m_file->close();
        }
        reply->deleteLater();
        if (aborted || status != 200) {
            QFile::remove(apkPath());
            setState(aborted ? QStringLiteral("available") : QStringLiteral("error"),
                     aborted ? QString() : status == 0 ? tr("No connection.") : tr("GitHub answered %1.").arg(status));
            return;
        }
        setState(QStringLiteral("ready"));
        install();
    });
}

void Updater::install()
{
    if (m_state != QLatin1String("ready"))
        return;
#ifdef Q_OS_ANDROID
    const QString result = ::android::install_apk(apkPath());
    if (result == QLatin1String("permission")) {
        setState(QStringLiteral("ready"), tr("Allow Pegasus X to install apps (the setting just opened), then pick Install again."));
        return;
    }
    if (!result.isEmpty())
        setState(QStringLiteral("ready"), result);
#endif
}

void Updater::cancel()
{
    if (m_reply)
        m_reply->abort();
}
