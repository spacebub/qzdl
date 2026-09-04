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
#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QTimer>
// ReSharper disable once CppUnusedIncludeDirective
#include <QtQml/qqmlregistration.h>

#include "core/Process.h"

#ifndef _WIN32
class QSocketNotifier;
#endif

/*
What one game printed. The pipe is drained whatever is happening, because a
child whose output nobody takes stops as soon as it fills; what is optional is
telling anything about it. Nothing is looking most of the time, and then the
lines only go into the buffer.
*/
class RunLog : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.runs.log()")

    Q_PROPERTY(int count READ rowCount NOTIFY changed)

    /** Whether anything is showing it. Nothing is told about lines while nothing is. */
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

    /** True while the pipe is still open. */
    Q_PROPERTY(bool live READ live NOTIFY changed)

public:
    enum Role : std::uint16_t {
        LineRole = Qt::UserRole + 1,

        /** True for a line ZDL wrote about the run rather than one the game did. */
        OwnRole,
    };

    explicit RunLog(QObject *parent = nullptr);

    ~RunLog() override;

    RunLog(const RunLog &) = delete;
    RunLog &operator=(const RunLog &) = delete;
    RunLog(RunLog &&) = delete;
    RunLog &operator=(RunLog &&) = delete;

    /** Takes over a stream and reads it until it ends. */
    void watch(Process::Stream output);

    /** Puts one of ZDL's own lines in, in its place among the game's. */
    void note(const QString &text);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool active() const { return _active; }

    void setActive(bool value);

    [[nodiscard]] bool live() const { return _output != Process::NOTHING; }

    /** All of it as one string, for putting on the clipboard. */
    [[nodiscard]] Q_INVOKABLE QString text() const;

    Q_INVOKABLE void clear();

signals:
    void changed();

    void activeChanged();

private:
    /** Reads whatever is waiting and cuts it into lines. */
    void drain();

    void release();

    /** Hands the lines gathered since the last one to whatever is looking. */
    void publish();

    /** As far back as the console can be scrolled. */
    static constexpr int LIMIT = 4000;

    /** How long lines are gathered for before anything is told, in milliseconds. */
    static constexpr int BATCH_MS = 60;

#ifdef _WIN32
    /** Windows has nothing to wake on for a pipe, so it is looked at instead. */
    static constexpr int POLL_MS = 60;

    QTimer _poll;
#else
    QSocketNotifier *_notifier{nullptr};
#endif

    Process::Stream _output{Process::NOTHING};

    struct Line {
        QString text;
        bool own{false};
    };

    QList<Line> _lines;

    /** The part of the last read that had no newline on the end of it yet. */
    QString _partial;

    QList<Line> _pending;

    /** Lines arrived while nothing was looking, so the whole of it is stale. */
    bool _missed{false};

    bool _active{false};

    QTimer _batch;
};
