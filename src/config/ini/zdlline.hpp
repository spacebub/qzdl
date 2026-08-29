/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#pragma once

#include <cstdint>

#include <QString>

enum ZDLLineFlags : std::uint8_t {
    FLAG_NORMAL = 0,    // Normal flag
    FLAG_VIRTUAL = 1,   // Does not get read/written, not cloned
    FLAG_NOWRITE = 2,   // Can not write to this value
    FLAG_TEMP = 4,      // Read/write and cloned, but not written
};

class ZDLLine {
    friend class ZDLVariables;

public:
    explicit ZDLLine(const QString &inLine);

    ZDLLine();

    ~ZDLLine();

    ZDLLine(const ZDLLine &) = delete;

    ZDLLine &operator=(const ZDLLine &) = delete;

    ZDLLine(ZDLLine &&) = delete;

    ZDLLine &operator=(ZDLLine &&) = delete;

    static int getType();

    QString getValue();

    QString getVariable();

    QString getLine();

    void setValue(const QString &inValue);

    [[nodiscard]] ZDLLine *clone() const;

    void setIsCopy(bool val);

    bool setFlags(ZDLLineFlags flag);

    [[nodiscard]] ZDLLineFlags getFlags() const {
        return flags;
    }

private:
    bool isCopy;

    void parse();

    int findComment(char delim);

    int type;
    QString line;
    QString comment;
    QString value;
    QString variable;
    ZDLLineFlags flags{};
};
