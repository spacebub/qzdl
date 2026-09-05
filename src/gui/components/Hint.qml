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

    /*
    Where in the thing it belongs to it should point, which is normally where
    the pointer is. A tooltip sits over the middle of what it explains, and
    that is only near the pointer while that thing is small: over a label as
    wide as the page it lands half a page away from what was hovered.
    */
    property real at: -1

    x: {
        if (!hint.parent) {
            return 0
        }

        if (hint.at < 0) {
            return (hint.parent.width - hint.implicitWidth) / 2
        }

        // Over the pointer, and never off either end of what it belongs to.
        return Math.max(0, Math.min(hint.at - hint.implicitWidth / 2,
                                    hint.parent.width - hint.implicitWidth))
    }

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
