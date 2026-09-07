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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <utility>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "core/Archive.h"
#include "core/Text.h"
#include "gui/Browse.h"
#include "gui/PathText.h"

namespace {

// Long enough for a slow mirror, short enough that a dead one gives up.
constexpr int STALL_MS = 30000;

const char *AGENT = "qzdl/" QZDL_VERSION;

QString text(const std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QNetworkRequest request(const QString &url, const bool json) {
    QNetworkRequest asked{QUrl(url)};

    asked.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(AGENT));
    asked.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
    asked.setTransferTimeout(STALL_MS);

    if (json) {
        asked.setRawHeader("Accept", "application/vnd.github+json");
    }

    return asked;
}

int status(const QNetworkReply *reply) {
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

// What was fetched is written beside it, so a card can say what is here as
// well as what has been released since.
const char *STAMP = "/.zdl-version";

// The version out of a release tag, which every project spells its own way:
// "woof_15.3.0", "g4.14.2", "v0.29.4".
QString tidy(const QString &tag) {
    for (qsizetype at = 0; at < tag.length(); at++) {
        if (tag.at(at).isDigit()) {
            return tag.mid(at);
        }
    }

    return tag;
}

// A file name that is only ever what the port itself is called, whatever the
// other end says the download is called.
QString safeName(const QString &name, const QString &fallback) {
    QString out;

    for (const QChar each : name) {
        if (each.isLetterOrNumber() || each == '.' || each == '-' || each == '_') {
            out.append(each);
        }
    }

    return out.isEmpty() || out.startsWith('.') ? fallback : out;
}

}

Browse::Browse(Notifier *notifier, NameList *ports, QObject *parent)
    : QAbstractListModel(parent),
      _notifier(notifier),
      _ports(ports),
      _network(new QNetworkAccessManager(this)),
      _entries(Catalog::ports().size()) {
    for (size_t row = 0; row < _entries.size(); row++) {
        settle(static_cast<int>(row));
    }

    measure();
}

Browse::~Browse() {
    for (Entry &entry : _entries) {
        drop(entry, true);
    }
}

const Catalog::Port &Browse::port(const int row) {
    return Catalog::ports()[static_cast<size_t>(row)];
}

int Browse::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_entries.size());
}

QHash<int, QByteArray> Browse::roleNames() const {
    return {
        {NameRole, "name"},
        {BlurbRole, "blurb"},
        {HomepageRole, "homepage"},
        {StatusRole, "status"},
        {VersionRole, "version"},
        {HaveRole, "have"},
        {SizeRole, "size"},
        {ProgressRole, "progress"},
        {FileRole, "file"},
        {ErrorRole, "error"},
        {DosRole, "dos"},
    };
}

QVariant Browse::data(const QModelIndex &index, const int role) const {
    if (index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const Entry &entry = _entries[static_cast<size_t>(index.row())];
    const Catalog::Port &known = port(index.row());

    switch (role) {
        case NameRole:
            return text(known.name);
        case BlurbRole:
            return text(known.blurb);
        case HomepageRole:
            return text(known.homepage);
        case StatusRole:
            return entry.state;
        case VersionRole:
            return entry.version;
        case HaveRole:
            return entry.have;
        case SizeRole:
            return entry.size;
        case ProgressRole:
            return entry.progress;
        case FileRole:
            return entry.file;
        case ErrorRole:
            return entry.error;
        case DosRole:
            return known.dos;
        default:
            return {};
    }
}

bool Browse::checking() const {
    return std::ranges::any_of(_entries, [](const Entry &entry) {
        return entry.state == QLatin1String("checking");
    });
}

bool Browse::busy() const {
    return std::ranges::any_of(_entries, [](const Entry &entry) {
        return entry.state == QLatin1String("fetching")
            || entry.state == QLatin1String("unpacking");
    });
}

QString Browse::directory() {
    return PathText::fromPath(Catalog::directory());
}

QString Browse::downloads() {
    return PathText::fromPath(Catalog::downloads());
}

qint64 Browse::cached() const {
    return _cached;
}

void Browse::measure() {
    const std::filesystem::path shelf = Catalog::downloads();
    std::error_code code;
    qint64 held = 0;

    for (std::filesystem::directory_iterator walk(shelf, code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        if (walk->is_regular_file(asked)) {
            held += static_cast<qint64>(walk->file_size(asked));
        }
    }

    if (held != _cached) {
        _cached = held;

        emit cacheChanged();
    }
}

void Browse::clearDownloads() {
    const std::filesystem::path shelf = Catalog::downloads();
    std::error_code code;

    for (std::filesystem::directory_iterator walk(shelf, code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        std::filesystem::remove_all(walk->path(), asked);
    }

    measure();

    _notifier->info(QStringLiteral("Anything fetched again comes down the wire afresh"),
                    QStringLiteral("The downloads are empty"));
}

QString Browse::trouble() const {
    return _trouble;
}

void Browse::touch(const int row) {
    const QModelIndex at = index(row);

    emit dataChanged(at, at);
    emit changed();
}

void Browse::give(const int row, const QString &state, const QString &error) {
    Entry &entry = _entries[static_cast<size_t>(row)];

    entry.state = state;
    entry.error = error;

    touch(row);
}

void Browse::drop(Entry &entry, const bool keepFile) {
    if (entry.reply != nullptr) {
        QNetworkReply *reply = entry.reply;

        entry.reply = nullptr;
        (void)reply->disconnect();
        reply->abort();
        reply->deleteLater();
    }

    if (entry.sink != nullptr) {
        entry.sink->close();

        if (!keepFile) {
            entry.sink->remove();
        }

        delete entry.sink;
        entry.sink = nullptr;
    }
}

void Browse::settle(const int row) {
    const Catalog::Port &known = port(row);
    Entry &entry = _entries[static_cast<size_t>(row)];

    // Whatever it was before this, it is what is on disk now.
    entry.error.clear();
    entry.have.clear();
    entry.progress = 0;

    // Somewhere else entirely: there is a page to get it from and no build
    // here to point at.
    if (known.repository.empty() && known.file.empty()) {
        entry.state = QStringLiteral("elsewhere");

        return;
    }

    if (Catalog::pattern(known).empty()) {
        entry.state = QStringLiteral("unavailable");
        entry.error = QStringLiteral("There is no build of this one for this system");

        return;
    }

    // A build that never moves is known without asking anybody.
    if (!known.file.empty()) {
        entry.url = text(known.file);
        entry.asset = QUrl(entry.url).fileName();
        entry.version = text(known.version);
    }

    const std::filesystem::path where = Catalog::directory(known);
    const std::filesystem::path found = where.empty()
        ? std::filesystem::path()
        : Catalog::program(where, known.program, known.dos);

    if (!found.empty()) {
        QFile stamp(PathText::fromPath(where) + QLatin1String(STAMP));

        if (stamp.open(QIODevice::ReadOnly | QIODevice::Text)) {
            entry.have = QString::fromUtf8(stamp.readAll()).trimmed();
        }

        entry.file = PathText::fromPath(found);
        entry.state = QStringLiteral("installed");

        return;
    }

    entry.state = entry.url.isEmpty() ? QStringLiteral("waiting") : QStringLiteral("ready");
}

void Browse::refresh(const bool everything) {
    _trouble.clear();

    for (int row = 0; row < rowCount(); row++) {
        const Entry &entry = _entries[static_cast<size_t>(row)];

        if (entry.reply != nullptr || port(row).repository.empty()
            || Catalog::pattern(port(row)).empty()) {
            continue;
        }

        // One already fetched is asked about too, so that the card can say
        // what the project has released since.
        if (everything || entry.state == QLatin1String("waiting")
            || entry.state == QLatin1String("failed")
            || (entry.state == QLatin1String("installed") && entry.url.isEmpty())) {
            check(row);
        }
    }

    emit changed();
}

void Browse::check(const int row) {
    const Catalog::Port &known = port(row);
    Entry &entry = _entries[static_cast<size_t>(row)];

    if (known.repository.empty() || entry.reply != nullptr) {
        return;
    }

    const QString url = "https://api.github.com/repos/" + text(known.repository)
        + "/releases/latest";
    const bool held = entry.state == QLatin1String("installed");

    QNetworkReply *reply = _network->get(request(url, true));

    entry.reply = reply;

    if (!held) {
        entry.state = QStringLiteral("checking");
    }

    touch(row);

    connect(reply, &QNetworkReply::finished, this, [this, row, reply, held] {
        Entry &waiting = _entries[static_cast<size_t>(row)];

        if (waiting.reply != reply) {
            return;
        }

        waiting.reply = nullptr;
        reply->deleteLater();

        const bool wanted = std::exchange(waiting.wanted, false);
        const int code = status(reply);

        if (reply->error() != QNetworkReply::NoError) {
            /*
            GitHub answers a few dozen questions an hour from one address and
            then stops, which is the one failure worth naming: everything else
            is the network being the network.
            */
            _trouble = code == 403 || code == 429
                ? QStringLiteral("GitHub is not answering any more questions from here just now. "
                                 "Its limit lifts within the hour.")
                : reply->errorString();

            give(row, held ? QStringLiteral("installed") : QStringLiteral("failed"), _trouble);

            return;
        }

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const std::string_view pattern = Catalog::pattern(port(row));

        waiting.version = tidy(release.value("tag_name").toString());
        waiting.url.clear();
        waiting.asset.clear();
        waiting.size = 0;

        for (const QJsonValue each : release.value("assets").toArray()) {
            const QJsonObject asset = each.toObject();
            const QString name = asset.value("name").toString();

            if (Catalog::matches(name.toStdString(), pattern)) {
                waiting.asset = name;
                waiting.url = asset.value("browser_download_url").toString();
                waiting.size = static_cast<qint64>(asset.value("size").toDouble());

                break;
            }
        }

        if (waiting.url.isEmpty()) {
            give(row, held ? QStringLiteral("installed") : QStringLiteral("unavailable"),
                 QStringLiteral("The latest release has no build for this system"));

            return;
        }

        give(row, held ? QStringLiteral("installed") : QStringLiteral("ready"));

        if (wanted) {
            fetch(row);
        }
    });
}

void Browse::install(const int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    Entry &entry = _entries[static_cast<size_t>(row)];

    if (entry.reply != nullptr || entry.state == QLatin1String("unpacking")) {
        return;
    }

    // Nothing to fetch yet: ask what the latest release is and carry on from
    // the answer.
    if (entry.url.isEmpty()) {
        entry.wanted = true;

        check(row);

        return;
    }

    fetch(row);
}

void Browse::fetch(const int row) {
    const Catalog::Port &known = port(row);
    Entry &entry = _entries[static_cast<size_t>(row)];
    const std::filesystem::path shelf = Catalog::downloads();

    if (shelf.empty()) {
        give(row, QStringLiteral("failed"), QStringLiteral("There is nowhere to put it"));

        return;
    }

    std::error_code code;

    std::filesystem::create_directories(shelf, code);

    if (code) {
        give(row, QStringLiteral("failed"),
             QString::fromStdString("Could not make a directory for it: " + code.message()));

        return;
    }

    const QString name = safeName(entry.asset, text(known.id));
    const QString into = PathText::fromPath(shelf) + "/" + name;

    // The same build fetched before and kept: there is nothing to bring down.
    if (const QFileInfo held(into);
        held.isFile() && (entry.size <= 0 || held.size() == entry.size)) {
        unpack(row, into);

        return;
    }

    auto *sink = new QFile(into + ".part");

    if (!sink->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete sink;
        give(row, QStringLiteral("failed"),
             QStringLiteral("Could not write to ") + PathText::fromPath(shelf));

        return;
    }

    QNetworkReply *reply = _network->get(request(entry.url, false));

    entry.sink = sink;
    entry.reply = reply;
    entry.progress = 0;
    entry.state = QStringLiteral("fetching");

    touch(row);

    connect(reply, &QNetworkReply::readyRead, this, [this, row, reply] {
        const Entry &going = _entries[static_cast<size_t>(row)];

        if (going.reply == reply && going.sink != nullptr) {
            going.sink->write(reply->readAll());
        }
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, row, reply](const qint64 done, const qint64 total) {
                Entry &going = _entries[static_cast<size_t>(row)];

                if (going.reply != reply) {
                    return;
                }

                // The size the release named stands in while the transfer has
                // not said one of its own.
                const qint64 whole = total > 0 ? total : going.size;

                going.progress = whole > 0
                    ? static_cast<double>(done) / static_cast<double>(whole)
                    : 0;

                touch(row);
            });

    connect(reply, &QNetworkReply::finished, this, [this, row, reply, into] {
        Entry &going = _entries[static_cast<size_t>(row)];

        if (going.reply != reply) {
            return;
        }

        going.reply = nullptr;

        const QNetworkReply::NetworkError trouble = reply->error();
        const QString said = reply->errorString();

        if (going.sink != nullptr && trouble == QNetworkReply::NoError) {
            going.sink->write(reply->readAll());
        }

        reply->deleteLater();
        drop(going, trouble == QNetworkReply::NoError);

        if (trouble == QNetworkReply::OperationCanceledError) {
            QFile::remove(into + ".part");
            settle(row);
            touch(row);

            return;
        }

        if (trouble != QNetworkReply::NoError) {
            QFile::remove(into + ".part");
            give(row, QStringLiteral("failed"), said);
            _notifier->error(said, QStringLiteral("Could not fetch it"));

            return;
        }

        QFile::remove(into);

        if (!QFile::rename(into + ".part", into)) {
            QFile::remove(into + ".part");
            give(row, QStringLiteral("failed"), QStringLiteral("Could not keep the download"));

            return;
        }

        measure();
        unpack(row, into);
    });
}

void Browse::unpack(const int row, const QString &archive) {
    const Catalog::Port &known = port(row);
    const std::filesystem::path where = Catalog::directory(known);
    const QString name = QFileInfo(archive).fileName();

    give(row, QStringLiteral("unpacking"));

    std::error_code code;

    std::filesystem::create_directories(where, code);

    if (code) {
        give(row, QStringLiteral("failed"),
             QString::fromStdString("Could not make a directory for it: " + code.message()));

        return;
    }

    std::filesystem::path program;

    // A zip holds the port; anything else -- an AppImage, a program on its own
    // -- is already the thing that runs and is copied across as it is.
    if (Text::iendsWith(name.toStdString(), ".zip")) {
        std::string trouble;

        if (!Archive::extract(PathText::toPath(archive), where, &trouble)) {
            give(row, QStringLiteral("failed"), QString::fromStdString(trouble));
            _notifier->error(QString::fromStdString(trouble),
                             QStringLiteral("Could not unpack ") + name);

            return;
        }

        program = Catalog::program(where, known.program, known.dos);
    } else {
        const QString target = PathText::fromPath(where) + "/" + name;

        QFile::remove(target);

        if (!QFile::copy(archive, target)) {
            give(row, QStringLiteral("failed"), QStringLiteral("Could not put it in place"));

            return;
        }

        program = PathText::toPath(target);
    }

    if (program.empty()) {
        const QString said = "Nothing in " + name + " is named " + text(known.program)
            + ". It is kept in the downloads.";

        give(row, QStringLiteral("failed"), said);
        _notifier->error(said, QStringLiteral("Could not set ") + text(known.name) + " up");

        return;
    }

    // A zip packed on Windows carries no such thing, and a program that cannot
    // be run is not one.
    std::filesystem::permissions(program,
                                 std::filesystem::perms::owner_exec
                                 | std::filesystem::perms::group_exec
                                 | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add, code);

    adopt(row, PathText::fromPath(program));
}

void Browse::adopt(const int row, const QString &file) {
    Entry &entry = _entries[static_cast<size_t>(row)];
    const Catalog::Port &known = port(row);
    const QString name = text(known.name);
    const QString root = PathText::fromPath(Catalog::directory(known));
    const QString before = entry.file;

    entry.file = file;
    entry.have = entry.version;
    entry.progress = 0;

    if (QFile stamp(root + QLatin1String(STAMP));
        stamp.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        stamp.write(entry.version.toUtf8());
    }

    // A build named after its own version does not overwrite the one it
    // replaces, so the one it replaces goes.
    if (!before.isEmpty() && before != file && before.startsWith(root + "/")) {
        QFile::remove(before);
    }

    /*
    The same port fetched twice over is still one source port: whatever entry
    already points inside its directory is pointed at the new build rather than
    joined by a second one, so every profile on it follows.
    */
    int at = -1;
    bool listed = false;

    for (int each = 0; each < _ports->rowCount(); each++) {
        const QString held = _ports->at(each).value("file").toString();

        if (held == file) {
            listed = true;

            break;
        }

        if (at < 0 && (held == before || held.startsWith(root + "/"))) {
            at = each;
        }
    }

    if (!listed && at >= 0) {
        _ports->update(at, _ports->at(at).value("name").toString(), file, known.dos);
    } else if (!listed) {
        _ports->add(file, name, known.dos);
    }

    give(row, QStringLiteral("installed"));

    _notifier->success(entry.version.isEmpty()
                           ? name + " is ready to use"
                           : name + " " + entry.version + " is ready to use",
                       QStringLiteral("Fetched"));
}

void Browse::cancel(const int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    Entry &entry = _entries[static_cast<size_t>(row)];

    if (entry.reply != nullptr) {
        entry.reply->abort();
    }
}

void Browse::erase(const int row) {
    const std::filesystem::path where = Catalog::directory(port(row));

    cancel(row);

    // Only ever the directory ZDL unpacked into, whatever else the port list
    // happens to point at.
    if (!where.empty()) {
        std::error_code code;

        std::filesystem::remove_all(where, code);
    }

    _entries[static_cast<size_t>(row)].file.clear();

    settle(row);
    touch(row);
}

void Browse::remove(const int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    const QString root = PathText::fromPath(Catalog::directory(port(row)));

    erase(row);

    for (int each = _ports->rowCount() - 1; each >= 0; each--) {
        if (_ports->at(each).value("file").toString().startsWith(root + "/")) {
            _ports->remove(each);
        }
    }
}

void Browse::forget(const int listed) {
    const QVariantMap held = _ports->at(listed);

    if (held.isEmpty()) {
        return;
    }

    const QString file = held.value("file").toString();

    for (int row = 0; row < rowCount(); row++) {
        const QString root = PathText::fromPath(Catalog::directory(port(row)));

        if (!root.isEmpty() && file.startsWith(root + "/")) {
            erase(row);

            break;
        }
    }

    _ports->remove(listed);
}
