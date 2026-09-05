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

// A small labelled value. It can be a way into whatever it names.
Column {
    id: fact

    property string label: ""
    property string value: ""
    property bool path: false
    property bool clickable: false
    property string hint: ""
    property int maximumWidth: 320

    signal activated()

    spacing: 3

    SectionLabel { text: fact.label.toUpperCase() }

    Item {
        width: fact.path ? fact.maximumWidth : Math.min(plain.implicitWidth, fact.maximumWidth)
        height: fact.path ? shown.implicitHeight : plain.implicitHeight

        PathLabel {
            id: shown
            visible: fact.path
            width: parent.width
            room: fact.maximumWidth
            path: fact.value
            color: fact.clickable && reach.hovered ? Theme.accent : Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold

            // The label carries the hover for both of them, and says the path
            // above it when it had to be cut down to fit.
            hint: shown.trimmed && fact.hint !== ""
                ? shown.pretty + "\n" + fact.hint
                : fact.hint
        }

        Text {
            id: plain
            visible: !fact.path
            width: parent.width
            text: fact.value
            color: fact.clickable && reach.hovered ? Theme.accent : Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            textFormat: Text.PlainText

            Behavior on color { ColorAnimation { duration: 120 } }
        }

        HoverHandler {
            id: reach
            enabled: fact.clickable
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            enabled: fact.clickable
            onTapped: fact.activated()
        }

        Hint {
            text: fact.hint
            visible: fact.hint !== "" && reach.hovered && !fact.path
        }
    }
}
