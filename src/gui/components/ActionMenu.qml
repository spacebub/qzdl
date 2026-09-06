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
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Zdl

/*
The actions that are not worth a button of their own: loading and saving
config files, clearing things out, the About box. They were a menu on a "ZDL"
button before and they are a menu now, since a list of unrelated verbs is
what a menu is for.

Each item is { action, label, glyph, danger, separator, enabled }, and a
separator is a row with nothing else on it.

What comes back is the item's action, not its position. A menu is a list that
gets things inserted into it, and a handler that switches on a row number is
one insertion away from firing the wrong thing at the wrong item.
*/
Popup {
    id: menu

    property var items: []

    signal triggered(string action)

    padding: 5
    modal: false

    /*
    Pressing the thing that opened it has to close it again. The default
    closes on any press outside the popup, which happens first and leaves
    the press to land on an opener that then opens it afresh; keeping the
    opener's own area out of that makes the pair a toggle.
    */
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    readonly property int rowHeight: 32
    readonly property int separatorHeight: 9

    width: 240
    height: {
        let total = 10

        for (let index = 0; index < menu.items.length; ++index) {
            total += menu.items[index].separator === true ? menu.separatorHeight : menu.rowHeight
        }

        return total
    }

    background: Rectangle {
        color: Theme.raised
        radius: Theme.radiusSmall
        border.width: 1
        border.color: Theme.borderStrong
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 110 }
    }

    contentItem: Column {
        spacing: 0

        Repeater {
            model: menu.items

            delegate: Item {
                id: row

                required property var modelData

                readonly property bool divider: row.modelData.separator === true
                readonly property bool usable: !row.divider && row.modelData.enabled !== false

                width: menu.width - 10
                height: row.divider ? menu.separatorHeight : menu.rowHeight

                Rectangle {
                    visible: row.divider
                    anchors.centerIn: parent
                    width: parent.width - 8
                    height: 1
                    color: Theme.border
                }

                Wash {
                    anchors.fill: parent
                    visible: !row.divider
                    hovered: hover.hovered && row.usable
                }

                Row {
                    visible: !row.divider
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 9

                    Glyph {
                        visible: row.modelData.glyph !== undefined
                        name: row.modelData.glyph === undefined ? "" : row.modelData.glyph
                        weight: 1
                        tone: row.modelData.danger === true ? Theme.danger : Theme.faint
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: row.divider ? "" : row.modelData.label
                        color: row.modelData.danger === true ? Theme.danger : Theme.text
                        opacity: row.usable ? 1 : 0.4
                        font.pixelSize: Theme.fontBody
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                    }
                }

                HoverHandler {
                    id: hover
                    enabled: row.usable
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    enabled: row.usable

                    onTapped: {
                        menu.close()
                        menu.triggered(row.modelData.action)
                    }
                }
            }
        }
    }
}
