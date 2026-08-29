/*
 * This file is part of qZDL
 * Copyright (C) 2019  Lcferrum
 * Copyright (C) 2023  spacebub
 * 
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <utility>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include "wad/ZLibDir.h"

ZLibDir::ZLibDir(QString file) :
        file(std::move(file)) {
}

ZLibDir::~ZLibDir()
= default;

QStringList ZLibDir::getMapNames() {
    QDir zdir(file);
    QStringList map_names;

    const QFileInfoList entries = zdir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &zname: entries) {
        if (ZDLMapFile *mapfile = getMapFile(zname.filePath())) {
            map_names += mapfile->getMapNames();
            delete mapfile;
        }
    }

    if (zdir.cd("maps")) {    //CD is case insensitive
        const QFileInfoList mapEntries = zdir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &zname: mapEntries) {
            map_names << zname.baseName().left(8).toUpper();
        }
    }

    return map_names;
}

QString ZLibDir::getIwadinfoName() {
    QDir const zdir(file);
    QString iwad_name;
    QStringList iwadinfo_filter;
    iwadinfo_filter << "iwadinfo" << "iwadinfo.*"; //QDir::Filter is case insensitive by default
    QFileInfoList iwadinfo_list = zdir.entryInfoList(iwadinfo_filter, QDir::Files | QDir::NoDotAndDotDot);

    if (!iwadinfo_list.empty()) {
        QFile iwadinfo_file(iwadinfo_list.first().filePath());

        if (iwadinfo_file.open(QIODevice::ReadOnly)) {
            static const QRegularExpression name_re("\\s+Name\\s*=\\s*\"(.+)\"\\s+");
            QRegularExpressionMatch const match = name_re.match(iwadinfo_file.readAll(), Qt::CaseInsensitive);

            if (match.hasPartialMatch()) {
                iwad_name = match.captured(1);
            }

            iwadinfo_file.close();
        }
    }

    return iwad_name;
}

bool ZLibDir::isMAPXX() {
    QDir zdir(file);
    bool is_mapxx = false;

    if (zdir.cd("maps")) {    //CD is case insensitive
        const QStringList mapNames = zdir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QString &zname: mapNames) {
            if ((zname.compare("map01.wad", Qt::CaseInsensitive) == 0)
                || (zname.compare("map01.map", Qt::CaseInsensitive) == 0)) {
                is_mapxx = true;
                break;
            }
        }
    }

    return is_mapxx;
}
