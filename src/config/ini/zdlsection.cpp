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

#include <QRegularExpression>
#include <utility>
#include "core/zdlcommon.h"
#include "config/ini/zdlsection.hpp"

#define READLOCK() (readLock(__FILE__,__LINE__))
#define WRITELOCK() (writeLock(__FILE__,__LINE__))
#define READUNLOCK() (releaseReadLock(__FILE__,__LINE__))
#define WRITEUNLOCK() (releaseWriteLock(__FILE__,__LINE__))

ZDLSection::ZDLSection(QString name) :
        mutex(LOCK_BUILDER()),
        sectionName(std::move(name)) {
}

ZDLSection::~ZDLSection() {
    WRITELOCK();
    while (!lines.empty()) {
        const ZDLLine *line = lines.front();
        lines.pop_front();
        delete line;
    }
    WRITEUNLOCK();
    delete mutex;
}

void ZDLSection::setSpecial(const ZDLLineFlags inFlags) {
    flags = inFlags;
}

int ZDLSection::hasVariable(const QString &variable) {
    reads++;
    READLOCK();
    for (auto *line: std::as_const(lines)) {
        if (line->getVariable().compare(variable) == 0) {
            READUNLOCK();
            return 1;
        }
    }
    READUNLOCK();
    return 0;
}

void ZDLSection::deleteVariable(const QString &variable) {
    WRITELOCK();
    reads++;
    for (int i = 0; i < lines.size(); i++) {
        ZDLLine *line = lines[i];
        if (line->getVariable().compare(variable) == 0) {
            lines.remove(i);
            delete line;
            WRITEUNLOCK();
            return;
        }
    }
    WRITEUNLOCK();
}

QString ZDLSection::findVariable(const QString &variable) {
    reads++;
    READLOCK();
    for (auto *line: std::as_const(lines)) {
        if (line->getVariable().compare(variable) == 0) {
            QString val(line->getValue());
            READUNLOCK();
            return val;
        }
    }
    READUNLOCK();
    return {""};
}

int ZDLSection::getRegex(const QString &regex, QVector<ZDLLine *> &vctr) {
#ifdef QT_CORE_LIB
    QRegularExpression const rx(regex);
    READLOCK();

    for (auto *line: std::as_const(lines)) {
        QRegularExpressionMatch const match = rx.match(line->getVariable());
        if (match.hasMatch()) {
            ZDLLine *copy = line->clone();
            copy->setIsCopy(true);
            vctr.push_back(copy);
        }
    }
    READUNLOCK();
    return static_cast<int>(vctr.size());
#else
    return 0;
#endif
}

int ZDLSection::setValue(const QString &variable, const QString &value) {
    writes++;
    WRITELOCK();

    for (auto *line: std::as_const(lines)) {
        if (line->getVariable().compare(variable) == 0) {
            if ((line->getFlags() & FLAG_NOWRITE) == FLAG_NOWRITE) {
                LOGDATAO() << "Cannot change value of FLAG_NOWRITE" << Qt::endl;
                return -1;
            }
            line->setValue(value);
            WRITEUNLOCK();
            return 0;
        }
    }
    //We can't find the line, create a new one.
    QString buffer = variable;
    buffer.append("=");
    buffer.append(value);
    auto *line = new ZDLLine(buffer);
    lines.push_back(line);
    WRITEUNLOCK();
    return 0;
}

int ZDLSection::streamWrite(QIODevice *stream) {

#ifdef _WIN32
    QString el("\r\n");
#define ENDOFLINE el
#else
    QString const el("\n");
#define ENDOFLINE el
#endif
    READLOCK();
    QTextStream tstream(stream);
    //Write only if we have stuff to write
    if (!lines.empty()) {
        writes++;
        //Global's don't have a section name
        if (sectionName.length() > 0) {
            tstream << "[" << sectionName << "]" << ENDOFLINE;
        }
        for (auto *line: std::as_const(lines)) {
            if ((line->getFlags() & FLAG_VIRTUAL) == 0 && (line->getFlags() & FLAG_TEMP) == 0) {
                tstream << line->getLine() << ENDOFLINE;
            } else {
                LOGDATAO() << "Ignoring FLAG_VIRTUAL and FLAG_TEMP entries" << Qt::endl;
            }
        }
        tstream << ENDOFLINE;
    }
    READUNLOCK();
    return 0;
}

QString ZDLSection::getName() {
    reads++;
    return sectionName;
}

ZDLLine *ZDLSection::findLine(const QString &inVar) {
    READLOCK();
    for (auto *line: std::as_const(lines)) {
        if (line->getVariable().compare(inVar) == 0) {
            qDebug() << "UNSAFE OPERATION AT " << __FILE__ << ":" << __LINE__ << Qt::endl;
            READUNLOCK();
            return line;
        }
    }
    READUNLOCK();

    return nullptr;
}

int ZDLSection::addLine(const QString &data) {
    if (data.isEmpty()) {
        return 0;
    }

    writes++;
    auto *newl = new ZDLLine(data);
    WRITELOCK();
    ZDLLine *ptr = findLine(newl->getVariable());

    if (ptr == nullptr) {
        lines.push_back(newl);
        WRITEUNLOCK();
        return 0;
    }
    ptr->setValue(newl->getValue());
    delete newl;
    WRITEUNLOCK();
    return 1;
}

ZDLSection *ZDLSection::clone() {
    READLOCK();
    auto *copy = new ZDLSection(sectionName);
    for (const auto &line: lines) {
        /* Virtual flags do not get cloned */
        if ((line->getFlags() & FLAG_VIRTUAL) == 0) {
            copy->addLine(line->clone());
        } else {
            LOGDATAO() << "Ignoring clone request for virtual flags" << Qt::endl;
        }
    }
    READUNLOCK();
    return copy;
}

int ZDLSection::getFlagsForValue(const QString &var) {
    READLOCK();
    for (auto *line: std::as_const(lines)) {
        if (line->getVariable().compare(var) == 0) {
            return line->getFlags();
        }
    }
    READUNLOCK();
    return -1;
}

bool ZDLSection::setFlagsForValue(const QString &var, const ZDLLineFlags value) {
    READLOCK();
    for (auto *line: std::as_const(lines)) {
        if (line->getVariable().compare(var) == 0) {
            return line->setFlags(value);
        }
    }
    READUNLOCK();
    return false;
}

bool ZDLSection::deleteRegex(const QString &regex) {
    bool rc = false;
    WRITELOCK();
    QRegularExpression const rx(regex);

    for (int i = 0; i < lines.size(); i++) {
        ZDLLine *line = lines[i];
        QRegularExpressionMatch const match = rx.match(line->getVariable());

        if (match.hasMatch()) {
            lines.remove(i--);
            rc = true;
            delete line;
        }
    }

    WRITEUNLOCK();
    return rc;
}
