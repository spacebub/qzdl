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
import Zdl.Components

// The half of the engines page that hands out ports ZDL knows where to get:
// it fetches the build, unpacks it beside its own files and adds it to the
// list the profiles pick from.
Item {
    id: page

    property var confirm: null

    readonly property int bleed: 16
    readonly property int gutter: 16

    readonly property real room: sheet.width - page.bleed * 2 - (bar.visible ? bar.width : 0)
    readonly property int columns: Math.max(1, Math.floor((room + gutter) / (330 + gutter)))
    readonly property real cell: (room - (columns - 1) * gutter) / columns

    // Nothing is asked of GitHub until somebody comes looking.
    onVisibleChanged: {
        if (visible) {
            App.browse.refresh()
        }
    }

    function measure(bytes) {
        return bytes > 0 ? (bytes / 1048576).toFixed(1) + " MB" : ""
    }

    Flickable {
        id: sheet

        anchors.fill: parent
        contentWidth: width
        contentHeight: flow.y + flow.implicitHeight + kept.height + 34
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            id: bar
            visible: bar.size < 1
            width: 8

            contentItem: Rectangle {
                radius: 4
                color: Theme.borderStrong
                opacity: bar.pressed ? 0.9 : 0.45
            }
        }

        Grid {
            id: flow

            x: page.bleed
            y: 2
            width: page.room
            columns: page.columns
            columnSpacing: page.gutter
            rowSpacing: page.gutter

            Repeater {
                model: App.browse

                delegate: Surface {
                    id: card

                    required property int index
                    required property string name
                    required property string blurb
                    required property string homepage
                    required property string status
                    required property string version
                    required property string have
                    required property real size
                    required property real progress
                    required property string file
                    required property string error
                    required property bool dos

                    readonly property bool working: card.status === "fetching"
                                                 || card.status === "unpacking"
                    readonly property bool here: card.status === "installed"

                    // What is here is not what has been released since.
                    readonly property bool behind: card.here && card.have !== ""
                                                && card.version !== ""
                                                && card.version !== card.have

                    width: page.cell
                    height: 186
                    hoverable: true
                    hovered: reach.hovered

                    HoverHandler { id: reach }

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 8

                        Row {
                            width: parent.width
                            spacing: 8

                            Text {
                                width: parent.width
                                       - (marks.width > 0 ? marks.width + parent.spacing : 0)
                                text: card.name
                                color: Theme.text
                                font.pixelSize: Theme.fontMedium
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                                textFormat: Text.PlainText
                            }

                            Row {
                                id: marks

                                spacing: 6
                                anchors.verticalCenter: parent.verticalCenter

                                // A DOS program is run inside DOSBox, which is
                                // worth knowing before it is fetched.
                                Pill {
                                    visible: card.dos
                                    height: 22
                                    dot: false
                                    text: "DOS"
                                    tone: Theme.muted
                                    wash: Theme.mutedSoft
                                }

                                Pill {
                                    id: mark

                                    height: 22
                                    visible: card.status !== "ready"
                                             && card.status !== "waiting" && !card.working

                                    text: card.behind ? "Update"
                                        : card.here ? "Installed"
                                        : card.status === "checking" ? "Checking"
                                        : card.status === "failed" ? "Failed"
                                        : card.status === "elsewhere" ? "Its own site"
                                        : "Not for this system"

                                    tone: card.behind ? Theme.accent
                                        : card.here ? Theme.success
                                        : card.status === "failed" ? Theme.danger
                                        : card.status === "checking" ? Theme.accent
                                        : Theme.warning

                                    wash: card.behind ? Theme.accentSoft
                                        : card.here ? Theme.successSoft
                                        : card.status === "failed" ? Theme.dangerSoft
                                        : card.status === "checking" ? Theme.accentSoft
                                        : Theme.warningSoft
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            text: card.blurb
                            color: Theme.faint
                            font.pixelSize: Theme.fontSmall
                            wrapMode: Text.WordWrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                    }

                    // The line above the buttons: what there is to fetch,
                    // how far it has got, or what went wrong.
                    Item {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: row.top
                        anchors.margins: 16
                        anchors.bottomMargin: 10
                        height: 16

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !card.working

                            text: card.error !== "" ? card.error
                                : card.status === "elsewhere" ? "Fetched from its own site"
                                : card.behind ? card.have + " here · " + card.version
                                                + " released"
                                : card.here ? (card.have === "" ? "" : card.have + " here")
                                : card.version === "" ? ""
                                : card.size > 0
                                    ? card.version + " · " + page.measure(card.size)
                                    : card.version

                            color: card.status === "failed" ? Theme.danger : Theme.faint
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            visible: card.working
                            height: 6
                            radius: 3
                            color: Theme.sunken

                            Rectangle {
                                height: parent.height
                                radius: parent.radius
                                color: Theme.accent

                                // Unpacking has no number to it, so the bar
                                // stands full while it happens.
                                width: card.status === "unpacking"
                                    ? parent.width
                                    : parent.width * Math.max(0, Math.min(1, card.progress))

                                Behavior on width { NumberAnimation { duration: 120 } }
                            }
                        }
                    }

                    Row {
                        id: row

                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 16
                        spacing: 8

                        AppButton {
                            visible: card.working
                            text: "Stop"
                            glyph: "cross"
                            compact: true
                            onClicked: App.browse.cancel(card.index)
                        }

                        AppButton {
                            visible: !card.working && card.status !== "elsewhere"
                                     && card.status !== "unavailable"
                            text: card.behind ? "Update"
                                : card.here ? "Fetch again"
                                : "Install"

                            glyph: "download"
                            variant: card.here && !card.behind ? "default" : "primary"
                            compact: true
                            busy: card.status === "checking"

                            hint: card.here
                                ? "Fetch the latest build over the one that is here"
                                : "Fetch it and add it to the source ports"

                            onClicked: App.browse.install(card.index)
                        }

                        GlyphButton {
                            visible: card.here && !card.working
                            glyph: "folder"
                            outlined: true
                            anchors.verticalCenter: parent.verticalCenter
                            hint: "Open the directory it was unpacked into"
                            onClicked: App.reveal(App.directoryOf(card.file))
                        }

                        GlyphButton {
                            visible: card.here && !card.working
                            glyph: "trash"
                            outlined: true
                            hoverTone: Theme.danger
                            anchors.verticalCenter: parent.verticalCenter
                            hint: "Delete what was fetched and take it out of the port list"

                            onClicked: page.confirm.ask(
                                "Remove " + card.name + "?",
                                "Everything ZDL unpacked for it is deleted and it is taken "
                                + "out of the source ports. Profiles pointing at it are left "
                                + "without a port.",
                                "Remove it", true,
                                function () { App.browse.remove(card.index) })
                        }

                        AppButton {
                            visible: !card.here || card.working
                            text: "Project page"
                            variant: "ghost"
                            compact: true
                            onClicked: Qt.openUrlExternally(card.homepage)
                        }
                    }
                }
            }
        }

        Surface {
            id: kept

            x: page.bleed
            y: flow.y + flow.implicitHeight + 18
            width: page.room
            height: 76

            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 14

                Fact {
                    label: "Where they are kept"
                    value: App.browse.directory
                    path: true
                    clickable: true
                    hint: "Open the directory"
                    maximumWidth: parent.width - 200
                    anchors.verticalCenter: parent.verticalCenter
                    onActivated: App.reveal(App.browse.directory)
                }
            }
        }
    }
}
