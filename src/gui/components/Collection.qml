import QtQuick
import QtQuick.Controls.Basic
import Zdl

/*
One of the two lists kept once and picked from by every profile. The IWADs and
the source ports are the same shape and the same set of things to do to them,
so they are the same panel twice rather than two panels that happen to look
alike.
*/
Surface {
    id: panel

    property string title: ""
    property string blurb: ""
    property var list: null

    // Whether what is in this list can be a DOS program, which only the ports are.
    property bool dosbox: false

    property var filters: [ "*" ]
    property string remember: "general"

    property var pick: null
    property var confirm: null
    property var entry: null

    function add() {
        // One step: what is picked is what is added, each named after its file.
        panel.pick.openMany("Add to " + panel.title, panel.filters,
                            function (paths, dosbox) { panel.list.addAll(paths, dosbox) },
                            panel.remember,
                            panel.dosbox ? "DOS programs" : "",
                            "Everything added here is started inside DOSBox instead of "
                            + "being run as it is")
    }

    function edit(row) {
        const current = panel.list.at(row)

        panel.entry.ask("Edit " + current.name, panel.list, panel.filters, panel.remember,
                        current.name, current.file,
                        function (name, file, dosbox) {
                            panel.list.update(row, name, file, dosbox)
                        },
                        panel.dosbox ? current.dosbox : undefined)
    }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Row {
            width: parent.width
            spacing: 8

            Column {
                width: parent.width - tools.width - 8
                spacing: 2
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: panel.title
                    color: Theme.text
                    font.pixelSize: Theme.fontMedium
                    font.weight: Theme.headingWeight
                    textFormat: Text.PlainText
                }

                Text {
                    width: parent.width
                    text: panel.blurb
                    color: Theme.faint
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
            }

            Row {
                id: tools
                spacing: 1
                anchors.verticalCenter: parent.verticalCenter

                GlyphButton {
                    glyph: "plus"
                    hint: "Add one or several, each named after its own file"
                    onClicked: panel.add()
                }
            }
        }

        Surface {
            width: parent.width
            height: parent.height - parent.spacing - tools.height - 26
            inset: true

            EmptyState {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: panel.list.count === 0
                title: "Nothing here yet"
                body: "Add one with the button above. Every profile picks from this list."
            }

            ListView {
                id: rows

                anchors.fill: parent
                anchors.margins: 6
                clip: true
                spacing: 1
                model: panel.list

                move: Transition {
                    NumberAnimation { properties: "y"; duration: 160; easing.type: Easing.OutQuad }
                }

                displaced: Transition {
                    NumberAnimation { properties: "y"; duration: 160; easing.type: Easing.OutQuad }
                }

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

                delegate: Item {
                    id: row

                    required property string name
                    required property string file
                    required property string directory
                    required property bool missing
                    required property bool dosbox
                    required property int index

                    width: rows.width - (bar.visible ? bar.width + 4 : 0)
                    height: App.config.showPaths ? 48 : 36

                    Item {
                        id: content

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: row.width
                        height: row.height

                        Drag.active: grab.drag.active
                        Drag.source: row
                        Drag.hotSpot.x: content.width / 2
                        Drag.hotSpot.y: content.height / 2

                        states: State {
                            when: grab.drag.active

                            ParentChange { target: content; parent: rows }

                            AnchorChanges {
                                target: content
                                anchors.horizontalCenter: undefined
                                anchors.verticalCenter: undefined
                            }

                            PropertyChanges { content.z: 2 }
                        }

                        Wash {
                            anchors.fill: parent
                            rounding: Theme.radiusSmall - 2
                            hovered: hover.hovered
                            selected: grab.drag.active
                            tint: Theme.mutedSoft
                        }

                        MouseArea {
                            id: grab

                            width: 22
                            height: parent.height
                            anchors.left: parent.left
                            hoverEnabled: true
                            cursorShape: Qt.SizeVerCursor
                            drag.target: content
                            drag.axis: Drag.YAxis

                            Glyph {
                                anchors.centerIn: parent
                                name: "grip"
                                weight: 1
                                tone: grab.containsMouse || grab.drag.active
                                    ? Theme.muted : Theme.border
                            }
                        }

                        Column {
                            anchors.left: grab.right
                            anchors.leftMargin: 6
                            anchors.right: actions.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1

                            Row {
                                width: parent.width
                                spacing: 7

                                Text {
                                    width: Math.min(implicitWidth, parent.width
                                        - (row.missing ? 60 : 0)
                                        - (row.dosbox ? 34 : 0))
                                    text: row.name
                                    color: row.missing ? Theme.danger : Theme.text
                                    font.pixelSize: Theme.fontBody
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    visible: row.missing
                                    text: "missing"
                                    color: Theme.danger
                                    font.pixelSize: Theme.fontTiny
                                    font.weight: Font.DemiBold
                                    anchors.verticalCenter: parent.verticalCenter
                                    textFormat: Text.PlainText
                                }

                                Text {
                                    visible: row.dosbox
                                    text: "DOS"
                                    color: Theme.faint
                                    font.pixelSize: Theme.fontTiny
                                    font.weight: Font.DemiBold
                                    anchors.verticalCenter: parent.verticalCenter
                                    textFormat: Text.PlainText
                                }
                            }

                            PathLabel {
                                visible: App.config.showPaths
                                width: parent.width
                                room: parent.width
                                path: row.file
                            }
                        }

                        Row {
                            id: actions

                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1
                            opacity: hover.hovered ? 1 : 0

                            Behavior on opacity { NumberAnimation { duration: 110 } }

                            GlyphButton {
                                glyph: "edit"
                                size: 26
                                hint: "Rename"
                                onClicked: panel.edit(row.index)
                            }

                            GlyphButton {
                                glyph: "cross"
                                size: 26
                                hoverTone: Theme.danger
                                hint: "Remove from the list"
                                onClicked: panel.confirm.ask(
                                    "Remove \"" + row.name + "\"?",
                                    "It goes out of this list and out of every profile that named "
                                    + "it. The file itself is left where it is.",
                                    "Remove", true,
                                    function () { panel.list.remove(row.index) })
                            }
                        }

                        HoverHandler { id: hover }

                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onDoubleTapped: panel.edit(row.index)
                        }
                    }

                    DropArea {
                        anchors.fill: parent

                        onEntered: function (event) {
                            panel.list.moveTo(event.source.index, row.index)
                        }
                    }
                }
            }
        }
    }
}
