import QtQuick
import Zdl

// Two ways of looking at the same shelf, and one control that says which.
Rectangle {
    id: control

    /** Each is { key, label }. */
    property var options: []
    property string current: ""

    signal selected(string key)

    readonly property int index: {
        for (let each = 0; each < control.options.length; ++each) {
            if (control.options[each].key === control.current) {
                return each
            }
        }

        return 0
    }

    implicitWidth: row.implicitWidth + 8
    implicitHeight: Theme.control
    radius: height / 2
    color: Theme.sunken
    border.width: 1
    border.color: Theme.border

    // Each tab reports its own box. A repeater and a positioner do not agree
    // on child order, so indexing into the row lands under the wrong word.
    property real markX: 0
    property real markWidth: 0

    Rectangle {
        x: row.x + control.markX
        y: 4
        width: control.markWidth
        height: parent.height - 8
        radius: height / 2
        color: Theme.accentSoft
        border.width: 1
        border.color: Qt.alpha(Theme.accent, 0.45)
        visible: control.markWidth > 0

        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row

        x: 4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Repeater {
            model: control.options

            delegate: Item {
                id: tab

                required property var modelData
                required property int index

                readonly property bool active: control.index === tab.index

                width: label.implicitWidth + 34
                height: control.height - 8

                Text {
                    id: label

                    anchors.centerIn: parent
                    text: tab.modelData.label
                    color: tab.active ? Theme.accent : hover.hovered ? Theme.text : Theme.muted
                    font.pixelSize: Theme.fontBody
                    font.weight: tab.active ? Font.DemiBold : Font.Normal
                    textFormat: Text.PlainText

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                function report() {
                    if (tab.active) {
                        control.markX = tab.x
                        control.markWidth = tab.width
                    }
                }

                onActiveChanged: tab.report()
                onXChanged: tab.report()
                onWidthChanged: tab.report()
                Component.onCompleted: tab.report()

                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler { onTapped: control.selected(tab.modelData.key) }
            }
        }
    }
}
