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
    property var filters: [ "*" ]
    property string remember: "general"

    property var pick: null
    property var confirm: null
    property var entry: null

    function add() {
        panel.entry.ask("Add to " + panel.title, panel.list, panel.filters, panel.remember, "", "",
                        function (name, file) { panel.list.add(file, name) })
    }

    function edit(row) {
        const current = panel.list.at(row)

        panel.entry.ask("Edit " + current.name, panel.list, panel.filters, panel.remember,
                        current.name, current.file,
                        function (name, file) { panel.list.update(row, name, file) })
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
                    hint: "Add one"
                    onClicked: panel.add()
                }

                GlyphButton {
                    glyph: "download"
                    hint: "Add several at once, each named after its own file"
                    onClicked: panel.pick.openMany("Add to " + panel.title, panel.filters,
                                                   function (paths) { panel.list.addAll(paths) },
                                                   panel.remember)
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
                body: "Add one with the buttons above. Every profile picks from this list."
            }

            ListView {
                id: rows

                anchors.fill: parent
                anchors.margins: 6
                clip: true
                spacing: 1
                model: panel.list

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
                    required property int index

                    width: rows.width - (bar.visible ? bar.width + 4 : 0)
                    height: App.config.showPaths ? 48 : 36

                    Wash {
                        anchors.fill: parent
                        rounding: Theme.radiusSmall - 2
                        hovered: hover.hovered
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.right: actions.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1

                        Row {
                            width: parent.width
                            spacing: 7

                            Text {
                                width: Math.min(implicitWidth, parent.width - (row.missing ? 60 : 0))
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
                        }

                        PathLabel {
                            visible: App.config.showPaths
                            width: parent.width
                            room: parent.width
                            path: row.file
                            clickable: true
                            opens: row.directory
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
                            glyph: "up"
                            size: 26
                            enabled: row.index > 0
                            hint: "Move up"
                            onClicked: panel.list.move(row.index, -1)
                        }

                        GlyphButton {
                            glyph: "down"
                            size: 26
                            enabled: row.index < panel.list.count - 1
                            hint: "Move down"
                            onClicked: panel.list.move(row.index, 1)
                        }

                        GlyphButton {
                            glyph: "edit"
                            size: 26
                            hint: "Rename, or point it at another file"
                            onClicked: panel.edit(row.index)
                        }

                        GlyphButton {
                            glyph: "cross"
                            size: 26
                            hoverTone: Theme.danger
                            hint: "Remove from the list"
                            onClicked: panel.confirm.ask(
                                "Remove \"" + row.name + "\"?",
                                "It goes out of this list and out of every profile that named it. The "
                                + "file itself is left where it is.",
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
            }
        }
    }
}
