import QtQuick
import QtQuick.Controls.Basic
import Zdl

/*
One of a list of things. Doom has a great many of these -- a skill, a map, a
game type, a number of players -- and they are all the same control so that
the launch page reads as one page rather than as a pile of unrelated widgets.

The list is opened over whatever is under it rather than pushing the page
about, and it is only as tall as it needs to be up to a point, past which it
scrolls.
*/
Item {
    id: control

    property var options: []
    property int current: -1

    // What is shown when nothing is picked. A skill or a map that is not set
    // means "leave it to the port", which is a thing to say rather than a blank.
    property string placeholder: "(Default)"
    property string label: ""
    property string hint: ""

    // Whether the list can be got out of without picking anything.
    property bool clearable: false

    readonly property bool open: list.visible
    readonly property string currentText: control.current >= 0 && control.current < control.options.length
        ? control.options[control.current]
        : ""

    signal selected(int index)

    implicitWidth: 200
    implicitHeight: column.implicitHeight
    opacity: enabled ? 1 : 0.45

    Column {
        id: column
        width: parent.width
        spacing: 6

        SectionLabel {
            text: control.label.toUpperCase()
            visible: control.label !== ""
        }

        Rectangle {
            id: box

            width: parent.width
            height: Theme.control
            radius: Theme.radiusSmall
            color: area.pressed ? Theme.sunken
                 : area.containsMouse ? Qt.tint(Theme.field, Theme.hover)
                 : Theme.field
            border.width: 1
            border.color: control.open ? Theme.accent
                        : area.containsMouse ? Theme.borderStrong
                        : Theme.border

            Behavior on color { ColorAnimation { duration: 110 } }
            Behavior on border.color { ColorAnimation { duration: 110 } }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.right: mark.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: control.currentText === "" ? control.placeholder : control.currentText
                color: control.currentText === "" ? Theme.faint : Theme.text
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }

            Glyph {
                id: mark
                name: "down"
                weight: 1
                tone: control.open ? Theme.accent : Theme.faint
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                rotation: control.open ? 180 : 0

                Behavior on rotation { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            }

            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: list.visible ? list.close() : list.open()
            }

            Hint {
                text: control.hint
                visible: control.hint !== "" && area.containsMouse && !control.open
            }
        }
    }

    Popup {
        id: list

        y: box.mapToItem(control, 0, box.height).y + 4
        width: control.width
        padding: 5
        modal: false

        // A long list of maps has to stop somewhere, and 9 rows is about as
        // far as one can be read down without losing where it started.
        readonly property int rows: Math.min(control.options.length + (control.clearable ? 1 : 0), 9)

        height: Math.max(44, list.rows * 32 + 10)

        background: Rectangle {
            color: Theme.raised
            radius: Theme.radiusSmall
            border.width: 1
            border.color: Theme.borderStrong
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 110 }
        }

        contentItem: ListView {
            id: rows

            clip: true
            currentIndex: control.current
            model: control.clearable
                ? [ control.placeholder ].concat(control.options)
                : control.options

            // The offset the clearing row introduces, in one place rather than
            // at every point that reads an index back out.
            readonly property int shift: control.clearable ? 1 : 0

            ScrollBar.vertical: ScrollBar {
                id: bar
                visible: bar.size < 1
                width: 7

                contentItem: Rectangle {
                    radius: 3.5
                    color: Theme.borderStrong
                    opacity: bar.pressed ? 0.9 : 0.45
                }
            }

            delegate: Item {
                id: row

                required property string modelData
                required property int index

                readonly property bool picked: row.index - rows.shift === control.current

                width: rows.width - (bar.visible ? bar.width + 2 : 0)
                height: 32

                Wash {
                    anchors.fill: parent
                    selected: row.picked
                    hovered: hover.hovered
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData
                    color: row.picked ? Theme.accent
                         : row.index < rows.shift ? Theme.faint
                         : Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: row.picked ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }

                TapHandler {
                    onTapped: {
                        control.selected(row.index - rows.shift)
                        list.close()
                    }
                }
            }

            Component.onCompleted: rows.positionViewAtIndex(control.current, ListView.Contain)
        }
    }
}
