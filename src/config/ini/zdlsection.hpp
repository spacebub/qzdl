/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
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
#pragma once

#include "core/zdlcommon.h"
#include "config/ini/zdlline.hpp"

class ZDLSection {
    friend class ZDLVariables;

public:
    explicit ZDLSection(QString name);

    ~ZDLSection();

    ZDLSection(const ZDLSection &) = delete;

    ZDLSection &operator=(const ZDLSection &) = delete;

    ZDLSection(ZDLSection &&) = delete;

    ZDLSection &operator=(ZDLSection &&) = delete;

    int addLine(const QString &data);

    QString getName();

    void setSpecial(ZDLLineFlags inFlags);

    QString findVariable(const QString &variable);

    int hasVariable(const QString &variable);

    void deleteVariable(const QString &variable);

    int setValue(const QString &variable, const QString &value);

    int streamWrite(QIODevice *stream);

    int getRegex(const QString &regex, QVector<ZDLLine *> &vctr);

    QVector<ZDLLine *> lines;

    ZDLSection *clone();

    void addLine(ZDLLine *line) {
        lines.push_back(line);
    }

    void setIsCopy(const bool copy) {
        isCopy = copy;
        for (const auto &line: lines) {
            line->setIsCopy(copy);
        }
    }

    int getFlagsForValue(const QString &var);

    bool setFlagsForValue(const QString &var, ZDLLineFlags value);

    bool deleteRegex(const QString &regex);

protected:
    void readLock(const char *file, const int line) const {
        LOGDATAO() << "ReadLockGet@" << file << ":" << line << Qt::endl;
        GET_READLOCK(mutex);
    }

    void writeLock(const char *file, const int line) const {
        LOGDATAO() << "WriteLockGet@" << file << ":" << line << Qt::endl;
        if (isCopy) {
            qDebug() << "WriteLock on copy from " << file << ":" << line << Qt::endl;
        }
        GET_WRITELOCK(mutex);
    }

    void releaseReadLock(const char *file, const int line) const {
        LOGDATAO() << "ReadLockRelease@" << file << ":" << line << Qt::endl;
        RELEASE_READLOCK(mutex);
    }

    void releaseWriteLock(const char *file, const int line) const {
        LOGDATAO() << "WriteLockRelease@" << file << ":" << line << Qt::endl;
        RELEASE_WRITELOCK(mutex);
    }

    bool tryReadLock(const char *file, const int line, const int timeout = 999999999) const {
        LOGDATAO() << "ReadLockTryGet@" << file << ":" << line << Qt::endl;
        return TRY_READLOCK(mutex, timeout);
    }

    bool tryWriteLock(const char *file, const int line, const int timeout = 999999999) const {
        LOGDATAO() << "WriteLockTryGet@" << file << ":" << line << Qt::endl;
        if (isCopy) {
            qDebug() << "WriteLock on copy" << Qt::endl;
        }
        return TRY_WRITELOCK(mutex, timeout);
    }

private:
    LOCK_CLASS *mutex;
    int reads{0};
    int writes{0};
    bool isCopy{false};

    ZDLLine *findLine(const QString &inVar);

    ZDLLineFlags flags{};
    QString sectionName;
};
