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

#include "gui/RunLog.h"

#ifndef _WIN32
#include <QSocketNotifier>
#endif

RunLog::RunLog(QObject *parent) : QAbstractListModel(parent) {
    _batch.setInterval(BATCH_MS);
    _batch.setSingleShot(true);

    connect(&_batch, &QTimer::timeout, this, &RunLog::publish);

#ifdef _WIN32
    _poll.setInterval(POLL_MS);

    connect(&_poll, &QTimer::timeout, this, &RunLog::drain);
#endif
}

RunLog::~RunLog() {
    release();
}

void RunLog::watch(const Process::Stream output) {
    release();

    if (output == Process::NOTHING) {
        return;
    }

    _output = output;

#ifdef _WIN32
    _poll.start();
#else
    _notifier = new QSocketNotifier(static_cast<int>(output), QSocketNotifier::Read, this);

    connect(_notifier, &QSocketNotifier::activated, this, &RunLog::drain);
#endif

    emit changed();
}

void RunLog::release() {
#ifdef _WIN32
    _poll.stop();
#else
    if (_notifier != nullptr) {
        // Called from the notifier's own signal, so it is not deleted outright.
        _notifier->setEnabled(false);
        _notifier->deleteLater();
        _notifier = nullptr;
    }
#endif

    if (_output != Process::NOTHING) {
        Process::closeStream(_output);
        _output = Process::NOTHING;
    }
}

int RunLog::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_lines.size());
}

QVariant RunLog::data(const QModelIndex &index, const int role) const {
    if (index.row() < 0 || index.row() >= _lines.size()) {
        return {};
    }

    const Line &line = _lines.at(index.row());

    switch (role) {
        case LineRole:
            return line.text;
        case OwnRole:
            return line.own;
        default:
            return {};
    }
}

QHash<int, QByteArray> RunLog::roleNames() const {
    return {{LineRole, "line"}, {OwnRole, "own"}};
}

void RunLog::note(const QString &text) {
    _pending.append(Line{.text = text, .own = true});

    if (!_active) {
        publish();

        return;
    }

    if (!_batch.isActive()) {
        _batch.start();
    }
}

void RunLog::setActive(const bool value) {
    if (value == _active) {
        return;
    }

    _active = value;

    /*
    Nothing was being told while nothing was looking, so what is in the buffer
    now bears no relation to what was last shown and the whole of it is read
    again. It is one reset however long the game has been running.
    */
    if (_active && _missed) {
        beginResetModel();
        _missed = false;
        endResetModel();

        emit changed();
    }

    emit activeChanged();
}

QString RunLog::text() const {
    QStringList out;

    out.reserve(_lines.size());

    for (const Line &line : _lines) {
        out.append(line.text);
    }

    return out.join(QLatin1Char('\n'));
}

void RunLog::clear() {
    beginResetModel();
    _lines.clear();
    _pending.clear();
    _partial.clear();
    _missed = false;
    endResetModel();

    emit changed();
}

void RunLog::drain() {
    std::string chunk;
    bool ended = false;

    while (true) {
        if (!Process::read(_output, chunk)) {
            ended = true;

            break;
        }

        // Nothing waiting, which is not the same as nothing ever again.
        if (chunk.empty()) {
            break;
        }

        _partial += QString::fromLocal8Bit(chunk.data(), static_cast<qsizetype>(chunk.size()));

        qsizetype cut = _partial.indexOf(QLatin1Char('\n'));

        while (cut >= 0) {
            QString line = _partial.left(cut);

            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }

            _pending.append(Line{.text = line, .own = false});
            _partial.remove(0, cut + 1);
            cut = _partial.indexOf(QLatin1Char('\n'));
        }
    }

    if (ended) {
        if (!_partial.isEmpty()) {
            _pending.append(Line{.text = _partial, .own = false});
            _partial.clear();
        }

        release();
        publish();

        emit changed();

        return;
    }

    if (_pending.isEmpty()) {
        return;
    }

    if (!_active) {
        publish();

        return;
    }

    // Gathered for a moment first: a game that talks a great deal would
    // otherwise be a model change for every line of it.
    if (!_batch.isActive()) {
        _batch.start();
    }
}

void RunLog::publish() {
    if (_pending.isEmpty()) {
        return;
    }

    if (!_active) {
        _lines.append(_pending);
        _pending.clear();

        if (const qsizetype over = _lines.size() - LIMIT; over > 0) {
            _lines.remove(0, over);
        }

        _missed = true;

        return;
    }

    if (const qsizetype over = _lines.size() + _pending.size() - LIMIT; over > 0) {
        const qsizetype dropped = std::min(over, _lines.size());

        beginRemoveRows({}, 0, static_cast<int>(dropped) - 1);
        _lines.remove(0, dropped);
        endRemoveRows();
    }

    const int at = static_cast<int>(_lines.size());

    beginInsertRows({}, at, at + static_cast<int>(_pending.size()) - 1);
    _lines.append(_pending);
    endInsertRows();

    _pending.clear();

    emit changed();
}
