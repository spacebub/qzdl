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

#include <QElapsedTimer>
// ReSharper disable once CppUnusedIncludeDirective
#include <QtQml/qqmlregistration.h>

#include "core/Process.h"
#include "gui/RunLog.h"

/*
What ZDL has started, filed under a name the caller chooses. There is no asking
a Doom port whether it has finished loading, so time stands in for it: still
alive a few seconds after starting means running, and gone means it fell over.

Every run also keeps a log, whether or not the game's own output is being taken,
because what ZDL did is worth having even when what the game said is not there.
Logs outlive the run, so a game that fell over can still be read afterwards.
*/
class Runs : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.runs")

    // By name: "launching", "running", "closed" or "failed".
    Q_PROPERTY(QVariantMap states READ states NOTIFY changed)

    Q_PROPERTY(bool busy READ busy NOTIFY changed)

    // The runs with a tab along the bottom of the window, oldest first.
    Q_PROPERTY(QStringList docked READ docked NOTIFY dockChanged)

    // Which of those is open. Empty is all of them folded away.
    Q_PROPERTY(QString showing READ showing NOTIFY dockChanged)

    // The names that have a log behind them, whether or not it has a tab.
    Q_PROPERTY(QStringList logged READ logged NOTIFY changed)

public:
    explicit Runs(QObject *parent = nullptr);

    // Something was started under this name. The stream is its output when it
    // was asked for, and the line is what ZDL is about to run.
    void began(const QString &key, const QString &title, const QString &commandLine,
               Process::Id id, Process::Stream output);

    void refused(const QString &key, const QString &title, const QString &reason);

    [[nodiscard]] QVariantMap states() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QStringList docked() const { return _docked; }
    [[nodiscard]] QString showing() const { return _showing; }
    [[nodiscard]] QStringList logged() const { return _logs.keys(); }

    [[nodiscard]] Q_INVOKABLE QString reason(const QString &key) const;

    // What the run is called, for a tab to say.
    [[nodiscard]] Q_INVOKABLE QString title(const QString &key) const;

    [[nodiscard]] Q_INVOKABLE bool alive(const QString &key) const;

    [[nodiscard]] Q_INVOKABLE RunLog *log(const QString &key) const;

    // Gives it a tab and opens it, whether or not it had one.
    Q_INVOKABLE void show(const QString &key);

    Q_INVOKABLE void hide();

    Q_INVOKABLE void toggle(const QString &key);

    // Takes the tab away, and stops the game if it is still going.
    Q_INVOKABLE void close(const QString &key);

signals:
    void changed();

    void dockChanged();

private:
    struct Run {
        Process::Id id{0};
        QString state;
        QString reason;
        QString title;

        // Set when ZDL asked it to quit, so its end is not read as a crash.
        bool asked{false};

        // When it last became whatever it is now.
        QElapsedTimer since;
    };

    void sweep();

    static void set(Run &run, const QString &state, const QString &reason = {});

    // Makes the log for this name if there is not one yet.
    RunLog *open(const QString &key);

    void dock(const QString &key);

    // How long a game is given to fall over before it is called running.
    static constexpr int SETTLE_MS = 4000;

    // How long an ended run is left on the card.
    static constexpr int CLOSED_MS = 5000;
    static constexpr int FAILED_MS = 15000;

    QHash<QString, Run> _runs;

    // These two outlive the run they came from, so that a log opened again a
    // long while later still knows what it belongs to.
    QHash<QString, RunLog *> _logs;
    QHash<QString, QString> _titles;

    QStringList _docked;
    QString _showing;

    // Still up, but no card is watching them: the same thing launched twice
    // over. Polled only so that they are reaped when they end.
    QList<Process::Id> _orphans;

    QTimer _clock;
};
