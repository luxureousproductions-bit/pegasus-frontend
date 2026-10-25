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


// Pegasus X: the Android helpers for the desktop test environment
// (qmake PEGASUSX_TESTENV=1), so the ES-DE import, ROM folders and Library
// code can run on Linux. The "device storage" is PEGASUSX_TEST_STORAGE (else
// the home folder); installed apps come from PEGASUSX_TEST_PACKAGES
// (comma separated package names).

#include "AndroidHelpers.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <functional>


namespace android {

const char* jni_classname() { return ""; }

QString primary_storage_path()
{
    const QString env = qEnvironmentVariable("PEGASUSX_TEST_STORAGE");
    return env.isEmpty() ? QDir::homePath() : QDir::cleanPath(env);
}

QStringList storage_paths() { return { primary_storage_path() }; }
bool has_external_storage_access() { return true; }
QString wait_for_storage(int) { return primary_storage_path(); }
QString last_storage_wait() { return {}; }

QString pdf_local_path(const QString& path_or_url, bool) { return path_or_url; }
int pdf_page_count(const QString&) { return 0; }

QStringList granted_paths() { return {}; }

QStringList installed_packages()
{
    return qEnvironmentVariable("PEGASUSX_TEST_PACKAGES").split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QStringList resolve_components(const QStringList& components) { return components; }
QString find_game_activity(const QString&) { return {}; }
QString app_label(const QString& package_name) { return installed_packages().contains(package_name) ? package_name : QString(); }
QStringList android_app_list() { return {}; }
void request_saf_permission(const std::function<void()>& cb) { if (cb) cb(); }

QString run_am_call(const QStringList&) { return QStringLiteral("Not on Android (test environment)"); }
QString to_content_uri(const QString& path) { return path; }
QString to_document_uri(const QString& path) { return path; }
QString install_apk(const QString&) { return QStringLiteral("Not on Android (test environment)"); }

} // namespace android
