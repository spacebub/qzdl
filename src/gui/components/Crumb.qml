import QtQuick
import Zdl

// One piece of a path, clickable.
Item {
    id: crumb

    property string label: ""
    property bool last: false

    signal activated()

    width: text.implicitWidth + 14
    height: parent ? parent.height : 30

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        radius: Theme.radiusSmall - 2
        color: hover.hovered ? Theme.hover : Qt.alpha(Theme.hover, 0)

        Behavior on color { ColorAnimation { duration: 100 } }
    }

    Text {
        id: text
        anchors.centerIn: parent
        text: crumb.label
        color: crumb.last ? Theme.text : hover.hovered ? Theme.accent : Theme.muted
        font.pixelSize: Theme.fontSmall
        font.family: Theme.mono
        font.weight: crumb.last ? Font.DemiBold : Font.Normal
        textFormat: Text.PlainText
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler { onTapped: crumb.activated() }
}
