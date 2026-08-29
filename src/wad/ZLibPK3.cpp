/*
 * This file is part of qZDL
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
#include <QFileInfo>
#include "wad/ZLibPK3.h"
#include "miniz.h"

ZLibPK3::ZLibPK3(QString file) :
        file(std::move(file)) {
}

ZLibPK3::~ZLibPK3()
= default;

QStringList ZLibPK3::getMapNames() {
    mz_zip_archive zip_archive = {};
    QStringList map_names;

    if (mz_zip_reader_init_file(&zip_archive, qPrintable(file), 0) != 0) {
        if (mz_uint const fnum = mz_zip_reader_get_num_files(&zip_archive)) {
            mz_zip_archive_file_stat file_stat;
            mz_uint mapinfo_idx{};
            bool mapinfo = false;
            bool zmapinfo = false;

            for (mz_uint i = 0; i < fnum; i++) {
                if ((mz_zip_reader_is_file_a_directory(&zip_archive, i) == 0)
                    && (mz_zip_reader_file_stat(&zip_archive, i, &file_stat) != 0)) {
                    QFileInfo const zname(file_stat.m_filename);
                    if (zname.path().compare("maps", Qt::CaseInsensitive) == 0) {
                        map_names << zname.baseName().left(8).toUpper();
                    } else if (zname.path().compare(".") == 0) {
                        if (!mapinfo && !zmapinfo && (zname.baseName().compare("mapinfo", Qt::CaseInsensitive) == 0)) {
                            mapinfo = true;
                            mapinfo_idx = i;
                        } else if (!zmapinfo && (zname.baseName().compare("zmapinfo", Qt::CaseInsensitive) == 0)) {
                            zmapinfo = true;
                            mapinfo_idx = i;
                        }
                    }
                }
            }

            if (mapinfo || zmapinfo) {
                size_t buf_len = 0;
                void *buf = nullptr;

                buf = mz_zip_reader_extract_to_heap(&zip_archive, mapinfo_idx, &buf_len, 0);
                if (buf != nullptr) {
                    QByteArray const char_buf = QByteArray::fromRawData(static_cast<const char *>(buf),
                                                                        static_cast<qsizetype>(buf_len));

                    static const QRegularExpression line_re("[\r\n]");
                    static const QRegularExpression name_re(R"(^\s*map\s+([^\s]+)(\s+.*)?$)");

                    const QStringList lines = QString(char_buf).split(line_re, Qt::SkipEmptyParts);
                    for (const QString &str: lines) {
                        QRegularExpressionMatch const match = name_re.match(str, Qt::CaseInsensitive);

                        if (match.hasPartialMatch()) {
                            map_names << match.captured(1).left(8).toUpper();
                        }
                    }

                    mz_free(buf);
                }
            }
        }
        mz_zip_reader_end(&zip_archive);
    }

    return map_names;
}

QString ZLibPK3::getIwadinfoName() {
    mz_zip_archive zip_archive = {};
    QString iwad_name;

    if (mz_zip_reader_init_file(&zip_archive, qPrintable(file), 0) != 0) {
        if (mz_uint const fnum = mz_zip_reader_get_num_files(&zip_archive)) {
            mz_zip_archive_file_stat file_stat;

            for (mz_uint i = 0; i < fnum; i++) {
                if ((mz_zip_reader_is_file_a_directory(&zip_archive, i) == 0)
                    && (mz_zip_reader_file_stat(&zip_archive, i, &file_stat) != 0)) {
                    QFileInfo const zname(file_stat.m_filename);
                    if ((zname.baseName().compare("iwadinfo", Qt::CaseInsensitive) == 0)
                        && (zname.path().compare(".") == 0)) {
                        size_t buf_len = 0;
                        void *buf = nullptr;
                        buf = mz_zip_reader_extract_to_heap(&zip_archive, i, &buf_len, 0);
                        if (buf != nullptr) {
                            QByteArray const char_buf = QByteArray::fromRawData(
                                    static_cast<const char *>(buf), static_cast<qsizetype>(buf_len));

                            static const QRegularExpression name_re("\\s+Name\\s*=\\s*\"(.+)\"\\s+");
                            QRegularExpressionMatch const match = name_re.match(char_buf, Qt::CaseInsensitive);

                            if (match.hasPartialMatch()) {
                                iwad_name = match.captured(1);
                            }

                            mz_free(buf);
                        }
                        break;
                    }
                }
            }
        }
        mz_zip_reader_end(&zip_archive);
    }

    return iwad_name;
}

bool ZLibPK3::isMAPXX() {
    mz_zip_archive zip_archive = {};
    bool is_mapxx = false;

    if (mz_zip_reader_init_file(&zip_archive, qPrintable(file), 0) != 0) {
        if (mz_uint const fnum = mz_zip_reader_get_num_files(&zip_archive)) {
            mz_zip_archive_file_stat file_stat;

            for (mz_uint i = 0; i < fnum; i++) {
                if ((mz_zip_reader_is_file_a_directory(&zip_archive, i) == 0)
                    && (mz_zip_reader_file_stat(&zip_archive, i, &file_stat) != 0)) {
                    QFileInfo const zname(file_stat.m_filename);
                    if ((zname.path().compare("maps", Qt::CaseInsensitive) == 0)
                        && ((zname.fileName().compare("map01.wad", Qt::CaseInsensitive) == 0)
                            || (zname.fileName().compare("map01.map", Qt::CaseInsensitive) == 0))) {
                        is_mapxx = true;
                        break;
                    }
                }
            }
        }
        mz_zip_reader_end(&zip_archive);
    }

    return is_mapxx;
}
