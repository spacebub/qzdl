/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cstdlib>

#include "config/ZDLJson.h"

#include <QSaveFile>
#include <utility>

#include "core/zdlcommon.h"

namespace ZDLJson {

Doc::~Doc() {
    if (doc != nullptr) {
        yyjson_doc_free(doc);
    }
}

Doc::Doc(Doc &&other) noexcept: doc(other.doc) {
    other.doc = nullptr;
}

Doc &Doc::operator=(Doc &&other) noexcept {
    if (this != &other) {
        if (doc != nullptr) {
            yyjson_doc_free(doc);
        }
        doc = other.doc;
        other.doc = nullptr;
    }
    return *this;
}

yyjson_val *Doc::root() const {
    return (doc != nullptr) ? yyjson_doc_get_root(doc) : nullptr;
}

Doc readData(const QByteArray &data, QString *error) {
    yyjson_read_err err{};
    yyjson_doc *doc = yyjson_read_opts(const_cast<char *>(data.constData()),
                                       static_cast<size_t>(data.size()),
                                       0, nullptr, &err);
    if (doc == nullptr) {
        if (error != nullptr) {
            *error = QString("%1 (at offset %2)")
                    .arg((err.msg != nullptr) ? err.msg : "unknown parse error")
                    .arg(static_cast<qulonglong>(err.pos));
        }
        return {};
    }
    return Doc(doc);
}

Doc readFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        LOGDATA() << "Cannot open JSON file " << path << Qt::endl;
        return {};
    }
    QByteArray const data = file.readAll();
    file.close();
    return readData(data, error);
}

yyjson_val *objGet(yyjson_val *obj, const char *key) {
    if ((obj == nullptr) || !yyjson_is_obj(obj)) {
        return nullptr;
    }
    return yyjson_obj_get(obj, key);
}

QString objGetString(yyjson_val *obj, const char *key, const QString &def) {
    yyjson_val *val = objGet(obj, key);
    if ((val == nullptr) || !yyjson_is_str(val)) {
        return def;
    }
    return QString::fromUtf8(yyjson_get_str(val), static_cast<qsizetype>(yyjson_get_len(val)));
}

int objGetInt(yyjson_val *obj, const char *key, const int def) {
    yyjson_val *val = objGet(obj, key);
    if (val == nullptr) {
        return def;
    }
    if (yyjson_is_int(val)) {
        return yyjson_get_int(val);
    }
    if (yyjson_is_real(val)) {
        return static_cast<int>(yyjson_get_real(val));
    }
    // Tolerate numbers that a hand edited config wrote as strings.
    if (yyjson_is_str(val)) {
        bool ok = false;
        int const parsed = QString::fromUtf8(yyjson_get_str(val)).toInt(&ok);
        return ok ? parsed : def;
    }
    return def;
}

bool objGetBool(yyjson_val *obj, const char *key, const bool def) {
    yyjson_val *val = objGet(obj, key);
    if (val == nullptr) {
        return def;
    }
    if (yyjson_is_bool(val)) {
        return yyjson_get_bool(val);
    }
    // Legacy configs stored flags as "1"/"0"; accept those too.
    if (yyjson_is_int(val)) {
        return yyjson_get_int(val) != 0;
    }
    if (yyjson_is_str(val)) {
        QString const str = QString::fromUtf8(yyjson_get_str(val));
        return str == "1" || str.compare("true", Qt::CaseInsensitive) == 0;
    }
    return def;
}

QStringList objGetStringList(yyjson_val *obj, const char *key) {
    QStringList out;
    yyjson_val *arr = objGet(obj, key);
    if ((arr == nullptr) || !yyjson_is_arr(arr)) {
        return out;
    }
    size_t idx = 0;
    size_t max = 0;
    yyjson_val *item = nullptr;
    yyjson_arr_foreach(arr, idx, max, item) {
        if (yyjson_is_str(item)) {
            out << QString::fromUtf8(yyjson_get_str(item), static_cast<qsizetype>(yyjson_get_len(item)));
        }
    }
    return out;
}

bool objGetIntArray(yyjson_val *obj, const char *key, int *out, const int count) {
    yyjson_val *arr = objGet(obj, key);
    if ((arr == nullptr) || !yyjson_is_arr(arr) || std::cmp_less(yyjson_arr_size(arr), count)) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        yyjson_val *item = yyjson_arr_get(arr, static_cast<size_t>(i));
        if ((item == nullptr) || !yyjson_is_num(item)) {
            return false;
        }
        out[i] = static_cast<int>(yyjson_get_num(item));
    }
    return true;
}

Builder::Builder() : doc(yyjson_mut_doc_new(nullptr)) {
}

Builder::~Builder() {
    if (doc != nullptr) {
        yyjson_mut_doc_free(doc);
    }
}

yyjson_mut_val *Builder::newObject() const {
    return yyjson_mut_obj(doc);
}

yyjson_mut_val *Builder::newArray() const {
    return yyjson_mut_arr(doc);
}

void Builder::setRoot(yyjson_mut_val *val) const {
    yyjson_mut_doc_set_root(doc, val);
}

void Builder::addString(yyjson_mut_val *obj, const char *key, const QString &value) const {
    if (obj == nullptr) {
        return;
    }
    QByteArray const utf8 = value.toUtf8();
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), yyjson_mut_strncpy(doc, utf8.constData(),
                                                                           static_cast<size_t>(utf8.size())));
}

void Builder::addInt(yyjson_mut_val *obj, const char *key, const int value) const {
    if (obj == nullptr) {
        return;
    }
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), yyjson_mut_int(doc, value));
}

void Builder::addBool(yyjson_mut_val *obj, const char *key, const bool value) const {
    if (obj == nullptr) {
        return;
    }
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), yyjson_mut_bool(doc, value));
}

void Builder::addValue(yyjson_mut_val *obj, const char *key, yyjson_mut_val *value) const {
    if ((obj == nullptr) || (value == nullptr)) {
        return;
    }
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), value);
}

void Builder::appendString(yyjson_mut_val *arr, const QString &value) const {
    if (arr == nullptr) {
        return;
    }
    QByteArray const utf8 = value.toUtf8();
    yyjson_mut_arr_append(arr, yyjson_mut_strncpy(doc, utf8.constData(), static_cast<size_t>(utf8.size())));
}

void Builder::appendInt(yyjson_mut_val *arr, const int value) const {
    if (arr == nullptr) {
        return;
    }
    yyjson_mut_arr_append(arr, yyjson_mut_int(doc, value));
}

void Builder::appendValue(yyjson_mut_val *arr, yyjson_mut_val *value) {
    if ((arr == nullptr) || (value == nullptr)) {
        return;
    }
    yyjson_mut_arr_append(arr, value);
}

bool Builder::writeFile(const QString &path, QString *error) const {
    yyjson_write_err werr{};
    size_t len = 0;
    char *json = yyjson_mut_write_opts(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, nullptr, &len, &werr);
    if (json == nullptr) {
        if (error != nullptr) {
            *error = (werr.msg != nullptr) ? QString::fromUtf8(werr.msg) : QString("unknown serialisation error");
        }
        LOGDATA() << "Failed to serialise JSON for " << path << Qt::endl;
        return false;
    }

    // The config directory may not exist yet on a first run.
    QFileInfo const info(path);
    QDir const dir = info.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (error != nullptr) {
            *error = QString("cannot create directory %1").arg(dir.path());
        }
        free(json);
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        free(json);
        LOGDATA() << "Cannot open JSON file for writing " << path << Qt::endl;
        return false;
    }
    file.write(json, static_cast<qint64>(len));
    file.write("\n", 1);
    free(json);

    if (!file.commit()) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        LOGDATA() << "Failed to commit JSON file " << path << Qt::endl;
        return false;
    }
    return true;
}

}
