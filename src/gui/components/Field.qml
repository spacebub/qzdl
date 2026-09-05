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

// A labelled text field. It can carry a button at its right edge, either a
// word or a glyph inside the box itself.
Item {
    id: control

    property string label: ""
    property string placeholder: ""
    property alias text: input.text
    property alias echoMode: input.echoMode
    property alias readOnly: input.readOnly
    property alias input: input
    property string action: ""
    property string prefix: ""
    property string icon: ""

    // A mark inside the left of the box. Drawn, not pressed, unlike the one at
    // the other end.
    property string leading: ""
    property string iconHint: ""
    property string hint: ""
    property bool mono: false

    // Something to say about the field itself, level with its label and out at
    // the right edge -- a pill saying where the value came from, so far.
    property Component labelBadge: null

    signal actionTriggered()
    signal accepted()

    implicitHeight: column.implicitHeight
    implicitWidth: 240

    Column {
        id: column
        width: parent.width
        spacing: 6

        Item {
            width: parent.width
            height: Math.max(caption.implicitHeight, badge.height)
            visible: control.label !== "" || control.labelBadge !== null

            SectionLabel {
                id: caption
                text: control.label.toUpperCase()
                visible: control.label !== ""
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }

            Loader {
                id: badge
                sourceComponent: control.labelBadge
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            width: parent.width
            spacing: 8

            Rectangle {
                id: box

                width: parent.width - (button.visible ? button.width + 8 : 0)
                height: Theme.control
                radius: Theme.radiusSmall
                color: Theme.field
                border.width: 1
                border.color: input.activeFocus ? Theme.accent : Theme.borderStrong

                Behavior on border.color { ColorAnimation { duration: 120 } }

                Glyph {
                    id: mark

                    visible: control.leading !== ""
                    name: control.leading
                    weight: 1.1
                    tone: input.activeFocus ? Theme.accent : Theme.faint
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: fixed
                    visible: control.prefix !== ""
                    text: control.prefix
                    color: Theme.faint
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.mono
                    anchors.left: mark.visible ? mark.right : parent.left
                    anchors.leftMargin: mark.visible ? 9 : 12
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                }

                /*
                The fixed part is not typed into, and run straight up against
                the part that is there is no telling where one ends. A rule
                between them says it, and the caret then starts on the far
                side of something rather than in the middle of a word.
                */
                Rectangle {
                    id: divider
                    visible: fixed.visible
                    width: 1
                    height: parent.height - 12
                    color: Theme.borderStrong
                    anchors.left: fixed.right
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                }

                TextField {
                    id: input
                    anchors.fill: parent
                    anchors.leftMargin: fixed.visible ? divider.x + 10
                                      : mark.visible ? mark.x + mark.width + 9
                                      : 12
                    anchors.rightMargin: inline.visible ? inline.width + 8 : 12
                    verticalAlignment: TextInput.AlignVCenter
                    placeholderText: control.placeholder
                    placeholderTextColor: Theme.faint
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.accentText
                    font.pixelSize: Theme.fontBody
                    font.family: control.mono ? Theme.mono : font.family
                    background: null
                    onAccepted: control.accepted()
                }

                // The glyph form of the button sits inside the box.
                GlyphButton {
                    id: inline
                    visible: control.icon !== ""
                    glyph: control.icon
                    size: 30
                    hint: control.iconHint
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: control.actionTriggered()
                }
            }

            AppButton {
                id: button
                visible: control.action !== ""
                text: control.action
                compact: true
                anchors.verticalCenter: parent.verticalCenter
                onClicked: control.actionTriggered()
            }
        }

        Text {
            width: parent.width
            visible: control.hint !== ""
            text: control.hint
            color: Theme.faint
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideMiddle
            maximumLineCount: 2
            textFormat: Text.PlainText
        }
    }
}
