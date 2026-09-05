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
import QtQuick.Window
import Zdl

/*
The window wears its own decoration, and since it is there anyway it carries
the navigation too. Dragging and the window buttons go through the window
manager, so a tiling one that ignores all three is not left with a bar that
pretends otherwise.
*/
Rectangle {
    id: bar

    property Window target: null
    property var pages: []
    property string current: ""
    property bool compact: false

    signal selected(string key)

    // A maximized frameless window covers the taskbar too, which is how Windows
    // ends up reporting it as FullScreen. Nothing here ever asks for fullscreen.
    readonly property bool maximized: target !== null
        && (target.visibility === Window.Maximized || target.visibility === Window.FullScreen)

    // Painted in the same colour as the page under it and parted by a hairline,
    // rather than being its own strip of chrome.
    implicitHeight: 54
    color: Theme.background

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    // Everything the controls do not claim is the drag handle.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: bar.target.startSystemMove()
        onDoubleClicked: bar.toggleMaximized()
    }

    Row {
        id: brand

        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 9

        Image {
            width: 22
            height: 22
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/qzdl-128.png"
            sourceSize.width: 44
            sourceSize.height: 44
            smooth: true
        }

        /*
        The name sits beside the icon rather than announcing itself, so it is
        weighted to be read and not to be shouted, which a near black bold at
        this size is on a white bar.
        */
        Text {
            visible: !bar.compact
            text: "ZDL"
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.Medium
            font.letterSpacing: 0.2
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
        }
    }

    /*
    Over the middle of the window, where the pages themselves are, rather than
    off in the left corner: a page is centred and only so wide, so a tab in the
    corner is the far end of a trip across the screen from anything it opens.
    A narrow window has no middle to spare, and there it falls back to sitting
    beside the name and keeps clear of the window buttons.
    */
    Row {
        id: tabs

        spacing: 2
        anchors.verticalCenter: parent.verticalCenter
        x: Math.max(brand.x + brand.width + 22,
                    Math.min((bar.width - tabs.width) / 2, controls.x - tabs.width - 16))

        Repeater {
            model: bar.pages

            delegate: Item {
                id: tab

                required property var modelData

                readonly property bool active: bar.current === tab.modelData.key

                width: label.implicitWidth + 30
                height: bar.height

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: tab.modelData.label
                    color: tab.active ? Theme.text : hover.hovered ? Theme.text : Theme.faint
                    font.pixelSize: Theme.fontBody
                    font.weight: tab.active ? Font.DemiBold : Font.Normal
                    textFormat: Text.PlainText

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                // A line along the bottom edge rather than a pill behind the
                // word: a pill reads as a button, and these are not buttons.
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: label.implicitWidth + 12
                    height: 2
                    radius: 1
                    color: Theme.accent
                    opacity: tab.active ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: 140 } }
                }

                Rectangle {
                    visible: tab.modelData.badge === true && !tab.active
                    width: 6
                    height: 6
                    radius: 3
                    color: Theme.accent
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.top: parent.top
                    anchors.topMargin: 13
                }

                HoverHandler { id: hover }

                TapHandler { onTapped: bar.selected(tab.modelData.key) }
            }
        }
    }

    Row {
        id: controls

        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        // Which shade the interface is painted in, and the one control for it.
        GlyphButton {
            glyph: Theme.mode
            anchors.verticalCenter: parent.verticalCenter
            hint: Theme.mode === "system" ? "Following the desktop. Click for the light theme"
                : Theme.mode === "light" ? "Light theme. Click for the dark one"
                : "Dark theme. Click to follow the desktop again"
            onClicked: Theme.cycle()
        }

        Row {
            spacing: 2
            anchors.verticalCenter: parent.verticalCenter

            GlyphButton {
                glyph: "minimize"
                onClicked: bar.target.showMinimized()
            }

            GlyphButton {
                glyph: bar.maximized ? "restore" : "maximize"
                onClicked: bar.toggleMaximized()
            }

            GlyphButton {
                glyph: "close"
                hoverWash: Theme.danger
                hoverTone: "#ffffff"
                onClicked: bar.target.close()
            }
        }
    }

    function toggleMaximized() {
        if (!bar.target) {
            return
        }

        if (bar.maximized) {
            bar.target.showNormal()
        } else {
            bar.target.showMaximized()
        }
    }
}
