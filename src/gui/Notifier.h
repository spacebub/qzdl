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

#include <algorithm>
#include <QObject>
// ReSharper disable once CppUnusedIncludeDirective
#include <QtQml/qqmlregistration.h> // Has to be here for the qml compiler

// Messages are buzzed into the corner of the window instead of interrupting
// with a box. Anything that is not an error takes itself off the screen again.
class Notifier : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.notify")

public:
    enum Severity { // NOLINT(*-enum-size) Q_ENUM does not like custom underlying types
        Info,
        Success,
        Warning,
        Error,
    };

    Q_ENUM(Severity)

    explicit Notifier(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void info(const QString &text, const QString &title = {}) {
        post(Info, text, title);
    }

    Q_INVOKABLE void success(const QString &text, const QString &title = {}) {
        post(Success, text, title);
    }

    Q_INVOKABLE void warning(const QString &text, const QString &title = {}) {
        post(Warning, text, title);
    }

    Q_INVOKABLE void error(const QString &text, const QString &title = {}) {
        post(Error, text, title);
    }

    // Errors stay until they are dismissed, everything else counts itself down.
    Q_INVOKABLE void post(const Notifier::Severity severity, const QString &text,
                          const QString &title = {}) {
        const int duration = severity == Error
            ? 0
            : 3200 + (static_cast<int>(std::min<qsizetype>(text.length(), 160)) * 18);

        emit posted(severity, title, text, duration);
    }

signals:
    void posted(Notifier::Severity severity, const QString &title, const QString &text,
                int duration);
};
