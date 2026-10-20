// Pegasus Frontend
// Copyright (C) 2017-2020  Mátyás Mustoha
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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringBuilder>


/// Pegasus X: launch command placeholders that read text from a file (the
/// way ES-DE's %INJECT% does), used by the launcher and by "Test with a game":
///   {file.contents}      the first line of the game file itself, eg. the
///                        Steam app ID in a .steam file
///   {file.inject:<ext>}  the first line of <game name>.<ext>, next to the
///                        game or inside a game folder, eg. the title ID in
///                        a PS Vita .psvita file
namespace game_file_text {

inline QString first_line(const QString& path)
{
    QFile file(path);
    if (!QFileInfo(path).isFile() || file.size() > 64 * 1024 || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed().section(QLatin1Char('\n'), 0, 0).trimmed();
}

inline void fill_in(QString& param, const QFileInfo& game)
{
    if (!param.contains(QLatin1String("{file.")))
        return;

    if (param.contains(QLatin1String("{file.contents}")))
        param.replace(QLatin1String("{file.contents}"), first_line(game.absoluteFilePath()));

    static const QRegularExpression rx_inject(QStringLiteral("\\{file\\.inject:(\\w+)\\}"));
    QRegularExpressionMatch match = rx_inject.match(param);
    while (match.hasMatch()) {
        const QString name = game.completeBaseName() % QLatin1Char('.') % match.captured(1);
        QString text;
        if (game.isDir())
            text = first_line(game.absoluteFilePath() % QLatin1Char('/') % name);
        if (text.isEmpty())
            text = first_line(game.absolutePath() % QLatin1Char('/') % name);
        param.replace(match.capturedStart(), match.capturedLength(), text);
        match = rx_inject.match(param, match.capturedStart() + text.length());
    }
}

} // namespace game_file_text
