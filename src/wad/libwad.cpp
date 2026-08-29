/*
 * This file is part of qZDL
 * Copyright (C) 2007-2012  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
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

#include <QRegularExpression>
#include <utility>
#include <fstream>
#include "wad/libwad.h"

DoomWad::DoomWad(QString file) :
        m_file(std::move(file)) {
}

DoomWad::~DoomWad()
= default;

QStringList DoomWad::getMapNames() {
    QStringList map_names;
    std::ifstream wadStream(m_file.toUtf8().constData(), std::ios::binary);

    if (!wadStream) {
        return map_names;
    }

    wadheader_t header{};
    wadStream.read((char *) &header, sizeof(header));

    std::vector<wadlump_t> lumps(header.numLumps);
    wadStream.seekg(header.directoryOffset);
    wadStream.read((char *) lumps.data(), (long) (header.numLumps * sizeof(wadlump_t)));

    // Generally the WAD structure follows a simple layout,
    // and we can assume that it will hold for most WADs.
    // In most cases map lumps follow the pattern:
    // MAPNAME
    // THINGS
    // ...
    // so we take the first lump name preceding THINGS
    const char *previous = nullptr;
    for (const wadlump_t &lump: lumps) {
        if (strcmp("THINGS", lump.name) == 0 && previous != nullptr) {
            map_names << previous;
        }

        previous = lump.name;
    }

    wadStream.close();
    return map_names;
}

QString DoomWad::getIwadinfoName() {
    QString iwadInfoName;
    std::ifstream wadStream(m_file.toUtf8().constData(), std::ios::binary);

    if (!wadStream) {
        return iwadInfoName;
    }

    wadheader_t header{};
    wadStream.read((char *) &header, sizeof(header));

    std::vector<wadlump_t> lumps(header.numLumps);
    wadStream.seekg(header.directoryOffset);
    wadStream.read((char *) lumps.data(), (long) (header.numLumps * sizeof(wadlump_t)));

    for (const wadlump_t &lump: lumps) {
        if (strcmp("IWADINFO", lump.name) == 0) {
            char *iwadinfo = new char[lump.length];
            wadStream.seekg(lump.offset);
            wadStream.read(iwadinfo, lump.length);

            static QRegularExpression name_re("\\s+Name\\s*=\\s*\"(.+)\"\\s+");
            QRegularExpressionMatch match = name_re.match(iwadinfo, Qt::CaseInsensitive);

            if (match.hasPartialMatch()) {
                iwadInfoName = match.captured(1);
            }

            delete[] iwadinfo;
            break;
        }
    }

    wadStream.close();
    return iwadInfoName;
}

bool DoomWad::isMAPXX() {
    bool isMapxx = false;
    std::ifstream wadStream(m_file.toUtf8().constData(), std::ios::binary);

    if (!wadStream) {
        return isMapxx;
    }

    wadheader_t header{};
    wadStream.read((char *) &header, sizeof(header));

    std::vector<wadlump_t> lumps(header.numLumps);
    wadStream.seekg(header.directoryOffset);
    wadStream.read((char *) lumps.data(), (long) (header.numLumps * sizeof(wadlump_t)));

    for (const wadlump_t &lump: lumps) {
        if (strcmp("MAP01", lump.name) == 0) {
            isMapxx = true;
            break;
        }
    }

    wadStream.close();
    return isMapxx;
}
