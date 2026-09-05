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
import QtQuick
import Zdl

// Messages pile up from the bottom corner, newest closest to the edge.
Item {
    id: stack

    property int limit: 4

    function post(severity, title, text, duration) {
        if (messages.count >= limit) {
            messages.remove(0)
        }

        messages.append({ severity: severity, title: title, body: text, duration: duration })
    }

    ListModel {
        id: messages
    }

    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 10

        Repeater {
            model: messages

            delegate: Buzz {
                required property var model
                required property int index

                severity: model.severity
                title: model.title
                body: model.body
                duration: model.duration

                onDismissed: messages.remove(index)
            }
        }
    }
}
