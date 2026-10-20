// Pegasus Frontend
// Copyright (C) 2017-2019  Mátyás Mustoha
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


#pragma once

#include <QString>


namespace android {

const char* jni_classname();

QString primary_storage_path();
QStringList storage_paths();
bool has_external_storage_access();
QString wait_for_storage(int timeout_ms);
QString last_storage_wait();

// PDF manuals (see PdfHelper.java and AndroidPdfProvider)
QString pdf_local_path(const QString& path_or_url, bool percent_encoded);
int pdf_page_count(const QString& path_or_url);

QStringList granted_paths();
QStringList installed_packages();
/// For each "package/activity": itself if it exists, else a screen of that
/// app that opens files (if installed), else empty; same length as the input
QStringList resolve_components(const QStringList&);
/// "package/activity" of an app's screen that opens game files, or empty
QString find_game_activity(const QString& package_name);
void request_saf_permission(const std::function<void()>&);

QString run_am_call(const QStringList&);
QString to_content_uri(const QString&);
QString to_document_uri(const QString&);

} // namespace android
