/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
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

#include "core/zdlcommon.h"
#include "config/ini/zdlline.hpp"

ZDLLine::ZDLLine(const QString &inLine) :
        isCopy(false),
        line(inLine.trimmed()),
        flags(FLAG_NORMAL) {
    if (line[0] == ';' || line[0] == '#') {
        type = 2;
    } else {
        type = 0;
        parse();
    }
}

ZDLLine::ZDLLine() :
        isCopy(false),
        type(2) {
}

ZDLLine::~ZDLLine()
= default;

void ZDLLine::setIsCopy(const bool val) {
    isCopy = val;
}

QString ZDLLine::getValue() {
    return QFD_QT_SEP(value);
}

QString ZDLLine::getVariable() {
    return QFD_QT_SEP(variable);
}

QString ZDLLine::getLine() {
    return QFD_QT_SEP(line);
}

void ZDLLine::setValue(const QString &inValue) {
    if (isCopy) {
        qDebug() << "SETTING A VALUE ON A COPY" << Qt::endl;
    }
    // Don't overwrite if the string is the same!
    if (inValue.compare(value, Qt::CaseInsensitive) == 0) {
        return;
    }

    line = variable + QString("=") + inValue;

    if (comment.size() > 0) {
        line = line + QString("     ") + comment;
    }

    value = inValue;
}

int ZDLLine::findComment(const char delim) {
    int const cloc = static_cast<int>(line.indexOf(delim, line.size()));
    if (cloc > -1) {
        if (cloc > 0) {
            if (line[cloc - 1] != '\\' && line[cloc - 1] != '/') {
                return cloc;
            }
        }
    }
    return -1;
}

void ZDLLine::parse() {
    int cloc = findComment(';');
    int const cloc2 = findComment('#');
    if (cloc != -1 && cloc2 != -1) {
        cloc = qMin(cloc, cloc2);
    } else if (cloc == -1 && cloc2 != -1) {
        cloc = cloc2;
    }

    if (cloc != -1) {
        //Strip out comment
        comment = line.mid(cloc, line.size()).trimmed();

    }

    int const loc = static_cast<int>(line.indexOf("=", 0));
    if (loc > -1) {
        variable = line.mid(0, loc).trimmed();
        value = line.mid(loc + 1, line.length() - loc - 1).trimmed();
        type = 0;
    } else {
        type = 1;
        variable = line;
        value = "";
    }

}

ZDLLine *ZDLLine::clone() const {
    auto *copy = new ZDLLine();
    copy->variable = variable;
    copy->comment = comment;
    copy->line = line;
    copy->value = value;
    copy->type = type;
    copy->flags = flags;
    return copy;
}

bool ZDLLine::setFlags(const int flag) {
    // Virtual flags cannot be modified
    if ((flags & FLAG_NOWRITE) == FLAG_NOWRITE) {
        LOGDATAO() << "Cannot change flags on FLAG_NOWRITE" << Qt::endl;
        return false;
    }
    flags = flag;
    return true;
}

int ZDLLine::getType() {
    return 0;
}
