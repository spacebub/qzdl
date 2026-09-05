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
import QtQuick.Controls.Basic
import Zdl

// A word about what a control does, for the ones where it is not obvious.
ToolTip {
    id: hint

    delay: 450
    padding: 10

    background: Rectangle {
        color: Theme.raised
        radius: Theme.radiusSmall
        border.width: 1
        border.color: Theme.borderStrong
    }

    contentItem: Text {
        text: hint.text
        color: Theme.text
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }
}
