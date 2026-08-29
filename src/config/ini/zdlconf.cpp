/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
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

/* ZDLConf.cpp
 *    Author: Cody Harris
 *   Purpose: To load in a configuration file, and parse it into sections and lines
 * Rationale: The current implementation of configuration files stores the values
 *            in a system which forgets keys and comments it doesn't know about.
 *            This system caches everything, and makes changes on the fly so that
 *            config files can be written back with keys it doesn't know how to use.
 *      Date: July 29th, 2007
 */
#include "core/zdlcommon.h"
#include "config/ini/zdlconf.hpp"

int ZDLConf::readINI(const QString &file) {
    LOGDATAO() << "Reading file " << file << Qt::endl;
    if ((mode & ZDLConf::FileRead) != 0) {
        writeLock();
        reads++;
        /* We allow lines to be outside of any section (ie header comments)
         * We use a global section to read this.  We also keep track of
         * which section we're in.  We do that with a pointer (current)
         */
        auto *current = new ZDLSection("");
        current->setSpecial(ZDL_FLAG_NAMELESS);
        sections.push_back(current);
        QFile stream(file);
        if (!stream.open(QIODevice::ReadOnly)) {
            LOGDATAO() << "Unable to open file" << Qt::endl;
            releaseWriteLock();
            return 1;
        }
        while (!stream.atEnd()) {
            QByteArray array = stream.readLine();
            QString line(array);
            parse(line, current);
            current = sections.back();
        }
        stream.close();

        QFileInfo info(file);
        if (!info.isWritable()) {
            LOGDATAO() << "File is unwriteable, writes will be ignored" << Qt::endl;
            mode = mode & ~FileWrite;
        }
        releaseWriteLock();
        return 0;
    } else {
        LOGDATAO() << "Cannot read file" << Qt::endl;
        return 1;
    }
}

int ZDLConf::writeINI(const QString &file) {
    setValue("zdl.general", "conflib", "sunrise");
    LOGDATAO() << "Writing file to " << file << Qt::endl;
    if ((mode & ZDLConf::FileWrite) != 0) {
        writes++;
        QFile stream(file);
        if (!stream.open(QIODevice::WriteOnly)) {
            QFileInfo fi(file);
            QDir dir = fi.dir();
            if (!dir.exists()) {
                dir.mkpath(".");
                if (!stream.open(QIODevice::WriteOnly)) {
                    LOGDATAO() << "Cannot create directory and cannot save file" << Qt::endl;
                    return 1;
                }
            } else {
                LOGDATAO() << "Cannot open file for writing" << Qt::endl;
                return 1;
            }
        }
        QIODevice *dev = &stream;
        writeStream(dev);
        stream.close();
        return 0;
    } else {
        LOGDATAO() << "Cannot write file, no permission" << Qt::endl;
        return 1;
    }
}

int ZDLConf::writeStream(QIODevice *stream) {
    if ((mode & ZDLConf::FileWrite) != 0) {
        readLock();
        for (auto &section: sections) {
            section->streamWrite(stream);
        }
        releaseReadLock();
        return 0;
    } else {
        return 1;
    }
}

ZDLConf::ZDLConf(int mode) {
    LOGDATAO() << "New ZDLConf" << Qt::endl;
    this->mode = mode;
    reads = 0;
    writes = 0;
    mutex = LOCK_BUILDER();
}

int ZDLConf::reopen(int imode) {
    LOGDATAO() << "Reopening with new permissions" << Qt::endl;
    this->mode = imode;
    return 0;
}

ZDLConf::~ZDLConf() {
    writeLock();
    LOGDATAO() << "Destroying ZDLConf" << Qt::endl;
    while ((int) sections.size() > 0) {
        ZDLSection *section = sections.front();
        sections.pop_front();
        delete section;
    }
    releaseWriteLock();
    delete mutex;
}

void ZDLConf::deleteSection(const QString &lsection) {
    LOGDATAO() << "Deleting section " << lsection << Qt::endl;
    writeLock();
    for (int i = 0; i < sections.size(); i++) {
        ZDLSection *section = sections[i];
        QString secName = section->getName();
        if (secName == lsection) {
            LOGDATAO() << "Found and removed" << Qt::endl;
            sections.remove(i);
            releaseWriteLock();
            return;
        }
    }
    releaseWriteLock();
    LOGDATAO() << "No such section" << Qt::endl;
}

void ZDLConf::deleteValue(const QString &lsection, const QString &variable) {
    LOGDATAO() << "Deleting value " << lsection << "/" << variable << Qt::endl;
    writeLock();
    if ((mode & WriteOnly) != 0) {
        writes++;
        for (auto section: sections) {
            if (section->getName().compare(lsection, Qt::CaseInsensitive) == 0) {
                LOGDATAO() << "Found section" << Qt::endl;
                section->deleteVariable(variable);
                releaseWriteLock();
                return;
            }

        }
    }
    releaseWriteLock();
}

QString ZDLConf::getValue(const QString &lsection, const QString &variable, int *status) {
    LOGDATAO() << "Getting value " << lsection << "/" << variable << Qt::endl;
    if ((mode & ReadOnly) != 0) {
        readLock();
        reads++;
        ZDLSection *sect = getSection(lsection);
        if (sect) {
            *status = 0;
            releaseReadLock();
            return sect->findVariable(variable);
        }
        *status = 1;
        releaseReadLock();
    }
    *status = 2;
    LOGDATAO() << "Failed to get value" << Qt::endl;
    return {};
}

QString ZDLConf::getValue(const QString &lsection, const QString &variable) {
    LOGDATAO() << "Getting value " << lsection << "/" << variable << Qt::endl;
    if ((mode & ReadOnly) != 0) {
        readLock();
        reads++;
        ZDLSection *sect = getSection(lsection);
        if (sect) {
            releaseReadLock();
            return sect->findVariable(variable);
        }
        releaseReadLock();
    }
    LOGDATAO() << "Failed to get value" << Qt::endl;
    return {};
}

ZDLSection *ZDLConf::getSection(const QString &lsection) {
    LOGDATAO() << "getting section " << lsection << Qt::endl;
    if ((mode & ReadOnly) != 0) {
        readLock();
        for (auto section: sections) {
            if (section->getName().compare(lsection, Qt::CaseInsensitive) == 0) {
                LOGDATAO() << "Got it " << DPTR(section) << Qt::endl;
                releaseReadLock();
                ZDLSection *copy = section->clone();
                copy->setIsCopy(true);
                return copy;
            }
        }
        releaseReadLock();
    }
    LOGDATAO() << "Failed to find section" << Qt::endl;
    return nullptr;
}

int ZDLConf::hasValue(const QString &lsection, const QString &variable) {
    LOGDATAO() << "hasValue: " << lsection << "/" << variable << Qt::endl;
    if ((mode & ReadOnly) != 0) {
        reads++;
        readLock();
        for (auto section: sections) {
            if (section->getName().compare(lsection, Qt::CaseInsensitive) == 0) {
                releaseReadLock();
                return section->hasVariable(variable);
            }

        }
        releaseReadLock();
    }
    LOGDATAO () << "No matching sections" << Qt::endl;
    return false;
}

void ZDLConf::setValue(const QString &lsection, const QString &variable, int value) {
    setValue(lsection, variable, QString::number(value));
}

void ZDLConf::setValue(const QString &lsection, const QString &variable, const QString &szBuffer) {
    LOGDATAO() << "setValue: " << lsection << "/" << variable << " = " << szBuffer << Qt::endl;
    if ((mode & WriteOnly) == 0) {
        return;
    }

    const QString &value = szBuffer;

    //Better handing of variables.  Don't overwrite if you don't have to.
    writeLock();
    if (hasValue(lsection, variable)) {
        int stat;
        QString oldValue = getValue(lsection, variable, &stat);
        if (oldValue == szBuffer) {
            LOGDATAO() << "No difference between set and previous variable" << Qt::endl;
            releaseWriteLock();
            return;
        }
    }

    writes++;
    for (auto section: sections) {
        if (section->getName().compare(lsection, Qt::CaseInsensitive) == 0) {
            section->setValue(variable, value);
            LOGDATAO() << "Asked section to set variable" << Qt::endl;
            releaseWriteLock();
            return;
        }
    }

    LOGDATAO() << "No such section, creating" << Qt::endl;
    //In this case, we didn't find the section
    auto *section = new ZDLSection(lsection);
    sections.push_back(section);
    section->setValue(variable, value);
    releaseWriteLock();
}

void ZDLConf::parse(QString in, ZDLSection *current) {
    if (in.length() < 1) {
        return;
    }

    in = in.trimmed();

    LOGDATAO() << "Parse " << in << Qt::endl;
    if (in.size() > 0
        && in[0] == '['
        && in[in.length() - 1] == ']') {
        in = in.mid(1, in.length() - 2);
        //This will remove duplicate sections automagically
        ZDLSection *ptr = getSection(in);
        if (ptr == nullptr) {
            current = new ZDLSection(in);
            sections.push_back(current);
        } else {
            current = ptr;
        }
    } else {
        current->addLine(in);
    }
}

ZDLConf *ZDLConf::clone() {
    LOGDATAO() << "Closing self" << Qt::endl;
    auto *copy = new ZDLConf();
    readLock();
    for (auto &section: sections) {
        copy->addSection(section->clone());
    }
    releaseReadLock();
    LOGDATAO() << "Cloned self to " << DPTR(copy) << Qt::endl;
    return copy;
}

void ZDLConf::deleteSectionByName(const QString &section) {
    LOGDATAO() << "Deleting section " << section << Qt::endl;
    writeLock();
    for (int i = 0; i < sections.size(); i++) {
        if (sections[i]->getName().compare(section) == 0) {
            ZDLSection *sect = sections[i];
            sections.remove(i);
            LOGDATAO() << "Deleted section" << Qt::endl;
            releaseWriteLock();
            delete sect;
        }
    }
    releaseWriteLock();
    LOGDATAO() << "No such section" << Qt::endl;
}

int ZDLConf::getFlagsForValue(const QString &lsection, const QString &var) {
    readLock();
    for (auto section: sections) {
        if (section->getName().compare(lsection, Qt::CaseInsensitive) == 0) {
            releaseReadLock();
            return section->getFlagsForValue(var);
        }
    }
    releaseReadLock();
    return -1;
}

bool ZDLConf::setFlagsForValue(const QString &lsection, const QString &var, int value) {
    readLock();
    for (auto section: sections) {
        if (section->getName().compare(lsection, Qt::CaseInsensitive) == 0) {
            releaseReadLock();
            return section->setFlagsForValue(var, value);
        }
    }
    releaseReadLock();
    return false;
}

bool ZDLConf::deleteRegex(const QString &lsection, const QString &regex) {
    readLock();
    for (auto section: sections) {
        if (section->getName().compare(lsection, Qt::CaseInsensitive) == 0) {
            bool rc = section->deleteRegex(regex);
            releaseReadLock();
            return rc;
        }
    }
    releaseReadLock();
    return false;
}


