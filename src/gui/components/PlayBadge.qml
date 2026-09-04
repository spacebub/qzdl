import QtQuick
import Zdl

// Sat over a card's picture, and only shown while the card is under the
// pointer.
Item {
    id: badge

    property bool shown: false

    property bool active: true
    property string hint: ""

    readonly property alias hovered: area.containsMouse

    signal clicked()

    implicitWidth: 48
    implicitHeight: 48

    // Faded rather than taken away: an item that is not there cannot report the
    // pointer arriving on it, which is what keeps the card hovered.
    opacity: badge.shown ? 1 : 0
    scale: badge.shown ? (area.pressed ? 0.94 : 1) : 0.7

    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 1.7
        height: parent.height * 1.7
        radius: width / 2
        color: Qt.alpha(Theme.ember, area.containsMouse ? 0.3 : 0.18)

        Behavior on color { ColorAnimation { duration: 140 } }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2

        gradient: Gradient {
            GradientStop { position: 0; color: Theme.emberHigh }
            GradientStop { position: 1; color: Theme.ember }
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.28)
        }

        Glyph {
            name: "play"
            weight: 1.5
            tone: Theme.artEdge
            anchors.centerIn: parent

            // The triangle sits heavy to the left of its own box.
            anchors.horizontalCenterOffset: 2
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: badge.active
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: badge.clicked()
    }

    Hint {
        text: badge.hint
        visible: badge.hint !== "" && area.containsMouse
    }
}
