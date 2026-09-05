import QtQuick
import Zdl

/*
An option that is either on or off. It was a box with a tick in it, which says
which way it is set only once you have looked at it closely; a switch says it
in where the knob sits, and reads from across the page.
*/
Item {
    id: control

    property string text: ""
    property bool checked: false
    property string hint: ""

    signal toggled(bool value)

    // The one way it changes, so that anything else offering to flip it -- a
    // whole card standing in as the click target -- goes the same way round.
    function toggle() {
        control.checked = !control.checked
        control.toggled(control.checked)
    }

    implicitWidth: row.implicitWidth
    implicitHeight: Math.max(24, row.implicitHeight)

    // A setting something else has settled still reads, it just cannot be
    // moved, and the hint is where the reason for that goes.
    opacity: control.enabled ? 1 : 0.45

    Behavior on opacity { NumberAnimation { duration: 120 } }

    Row {
        id: row
        spacing: 10
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            id: track

            width: 38
            height: 22
            radius: height / 2
            anchors.verticalCenter: parent.verticalCenter
            color: control.checked ? Theme.accent : Theme.sunken
            border.width: 1
            border.color: control.checked ? Theme.accent
                : area.containsMouse ? Theme.borderStrong : Theme.border

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            Rectangle {
                id: knob

                width: 16
                height: 16
                radius: width / 2
                y: 3
                x: control.checked ? track.width - width - 3 : 3
                color: control.checked ? Theme.accentText : Theme.faint

                Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                Behavior on color { ColorAnimation { duration: 120 } }
            }
        }

        Text {
            visible: control.text !== ""
            text: control.text
            color: Theme.text
            font.pixelSize: Theme.fontBody
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        enabled: control.enabled
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.toggle()
    }

    Hint {
        text: control.hint
        visible: control.hint !== "" && area.containsMouse
    }
}
