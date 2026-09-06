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
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components

/*
What runs the games: the ports that are set up, and the ones ZDL knows where
to get. They are two halves of one page rather than a page and a setting,
since getting one and having one is the same errand.
*/
Item {
    id: page

    property var pick: null
    property var confirm: null
    property var entry: null

    // installed | browse
    property string mode: "installed"

    readonly property bool showingInstalled: page.mode === "installed"

    readonly property int bleed: 16
    readonly property int gutter: 16

    readonly property real room: shelf.width - page.bleed * 2 - (bar.visible ? bar.width : 0)
    readonly property int columns: Math.max(1, Math.floor((room + gutter) / (330 + gutter)))
    readonly property real cell: (room - (columns - 1) * gutter) / columns

    function count(many, thing) {
        return many + " " + thing + (many === 1 ? "" : "s")
    }

    // With nothing set up there is nothing to show, so the page opens on the
    // half that has something to offer. Once, rather than as a binding: it is
    // where to start, not where to stay.
    Component.onCompleted: {
        if (App.config.ports.count === 0) {
            page.mode = "browse"
        }
    }

    // Adding by hand: one port, named after its own file. Several at once is
    // how files are added to a profile; an engine is picked one at a time.
    function add() {
        page.pick.open("Add a source port", App.portFilters, false,
                       function (path, dosbox) { App.config.ports.add(path, "", dosbox) },
                       "src", "DOS program",
                       "It is started inside DOSBox instead of being run as it is")
    }

    function edit(row) {
        const current = App.config.ports.at(row)

        page.entry.ask("Edit " + current.name, App.config.ports, App.portFilters, "src",
                       current.name, current.file,
                       function (name, file, dosbox) {
                           App.config.ports.update(row, name, file, dosbox)
                       },
                       current.dosbox)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: page.bleed
            Layout.rightMargin: page.bleed
            spacing: 14

            Column {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    text: "Engines"
                    color: Theme.text
                    font.pixelSize: Theme.fontDisplay
                    font.weight: Theme.headingWeight
                    textFormat: Text.PlainText
                }

                Text {
                    width: parent.width

                    text: page.showingInstalled
                        ? page.count(App.config.ports.count, "source port")
                          + " · what the profiles are run with"
                        : App.browse.trouble !== ""
                            ? App.browse.trouble
                            : "Fetched, unpacked and set up here · what each project has "
                              + "released is looked up on GitHub"

                    color: !page.showingInstalled && App.browse.trouble !== ""
                        ? Theme.danger
                        : Theme.faint

                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
            }

            AppButton {
                Layout.alignment: Qt.AlignVCenter
                visible: !page.showingInstalled
                text: "Check again"
                glyph: "refresh"
                compact: true
                busy: App.browse.checking
                hint: "Ask every project what it has released"
                onClicked: App.browse.refresh(true)
            }

            Segmented {
                Layout.alignment: Qt.AlignVCenter
                current: page.mode
                options: [
                    { key: "installed", label: "Installed" },
                    { key: "browse", label: "Get more" }
                ]
                onSelected: function (key) { page.mode = key }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: page.showingInstalled ? 0 : 1

            // What is set up, and one card that is the way to set up more.
            Flickable {
                id: shelf

                contentWidth: width
                contentHeight: flow.y + flow.implicitHeight + 24
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

                    move: Transition {
                        NumberAnimation {
                            properties: "x,y"
                            duration: 160
                            easing.type: Easing.OutQuad
                        }
                    }

                    Repeater {
                        model: App.config.ports

                        delegate: Slot {
                            id: slot

                            required property string name
                            required property string file
                            required property bool missing
                            required property bool dosbox

                            // One ZDL fetched itself, which is also the one it
                            // can throw away again.
                            readonly property bool fetched: slot.file.indexOf(
                                App.browse.directory + "/") === 0

                            width: page.cell
                            height: 152

                            // The card itself, which is what leaves the grid
                            // while it is being dragged.
                            Surface {
                                id: card

                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                width: slot.width
                                height: slot.height
                                hoverable: true
                                hovered: reach.hovered || carry.drag.active

                                Drag.active: carry.drag.active
                                Drag.source: slot

                                states: State {
                                    when: carry.drag.active

                                    ParentChange { target: card; parent: shelf }

                                    AnchorChanges {
                                        target: card
                                        anchors.horizontalCenter: undefined
                                        anchors.verticalCenter: undefined
                                    }

                                    PropertyChanges { card.z: 2 }
                                }

                                HoverHandler { id: reach }

                                // Under everything else on the card, so the
                                // buttons keep their own presses.
                                MouseArea {
                                    id: carry

                                    anchors.fill: parent
                                    drag.target: card

                                    // Where it is dropped is where the pointer
                                    // is, not where the middle of the card is.
                                    onPressed: function (mouse) {
                                        card.Drag.hotSpot.x = mouse.x
                                        card.Drag.hotSpot.y = mouse.y
                                    }

                                    onDoubleClicked: page.edit(slot.index)
                                }

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
                                                   - (marks.width > 0
                                                      ? marks.width + parent.spacing : 0)
                                            text: slot.name
                                            color: slot.missing ? Theme.danger : Theme.text
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

                                            Pill {
                                                visible: slot.dosbox
                                                height: 22
                                                dot: false
                                                text: "DOS"
                                                tone: Theme.muted
                                                wash: Theme.mutedSoft
                                            }

                                            Pill {
                                                visible: slot.missing
                                                height: 22
                                                text: "Missing"
                                                tone: Theme.danger
                                                wash: Theme.dangerSoft
                                            }
                                        }
                                    }

                                    PathLabel {
                                        width: parent.width
                                        room: parent.width
                                        path: slot.file
                                    }

                                    Text {
                                        width: parent.width
                                        visible: slot.fetched
                                        text: "Fetched by ZDL"
                                        color: Theme.faint
                                        font.pixelSize: Theme.fontSmall
                                        textFormat: Text.PlainText
                                    }
                                }

                                Row {
                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 16
                                    spacing: 8

                                    AppButton {
                                        text: "Edit"
                                        glyph: "edit"
                                        compact: true
                                        hint: "Rename it or point it at another file"
                                        onClicked: page.edit(slot.index)
                                    }

                                    GlyphButton {
                                        glyph: "folder"
                                        outlined: true
                                        anchors.verticalCenter: parent.verticalCenter
                                        hint: "Open the directory it is in"
                                        onClicked: App.reveal(App.directoryOf(slot.file))
                                    }

                                    GlyphButton {
                                        glyph: "trash"
                                        outlined: true
                                        hoverTone: Theme.danger
                                        anchors.verticalCenter: parent.verticalCenter

                                        hint: slot.fetched
                                            ? "Delete what was fetched and take it out of the list"
                                            : "Take it out of the list. The file itself is left "
                                              + "where it is"

                                        onClicked: page.confirm.ask(
                                            "Remove \"" + slot.name + "\"?",
                                            slot.fetched
                                                ? "Everything ZDL unpacked for it is deleted and it "
                                                  + "goes out of every profile that named it."
                                                : "It goes out of this list and out of every profile "
                                                  + "that named it. The file itself is left where it "
                                                  + "is.",
                                            "Remove", true,
                                            function () { App.browse.forget(slot.index) })
                                    }
                                }
                            }

                            // Stays where the card was while it is away, and
                            // catches whatever is dropped on it.
                            DropArea {
                                anchors.fill: parent

                                onEntered: function (event) {
                                    App.config.ports.moveTo((event.source as Slot).index,
                                                            slot.index)
                                }
                            }
                        }
                    }

                    // The way in for anything ZDL does not fetch itself.
                    Surface {
                        id: adder

                        width: page.cell
                        height: 152
                        inset: true
                        hoverable: true
                        hovered: reachAdd.hovered

                        HoverHandler {
                            id: reachAdd
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: page.add()
                        }

                        // It sits where the end of the list is, which is what
                        // dropping a card on it asks for.
                        DropArea {
                            anchors.fill: parent

                            onEntered: function (event) {
                                App.config.ports.moveTo((event.source as Slot).index,
                                                        App.config.ports.count - 1)
                            }
                        }

                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 40
                            spacing: 8

                            Glyph {
                                name: "plus"
                                weight: 1.6
                                tone: reachAdd.hovered ? Theme.accent : Theme.faint
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Text {
                                width: parent.width
                                text: "Add one you already have"
                                color: reachAdd.hovered ? Theme.text : Theme.muted
                                font.pixelSize: Theme.fontBody
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                textFormat: Text.PlainText
                            }

                            Text {
                                width: parent.width
                                text: "Point ZDL at a source port on this machine. It can be "
                                    + "marked as a DOS program while it is picked."
                                color: Theme.faint
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                textFormat: Text.PlainText
                            }
                        }
                    }
                }
            }

            BrowseView {
                confirm: page.confirm
            }
        }
    }
}
