import QtQuick
import Zdl

// Window and inline icon buttons: a glyph, a wash under it and nothing else.
Item {
    id: control

    property alias glyph: mark.name
    property color tone: Theme.muted
    property color hoverTone: Theme.text
    property color hoverWash: Theme.hover
    property int size: Theme.controlSmall
    property string hint: ""

    /*
    A glyph on its own is enough for a window control or for the mark inside
    a field, where what it belongs to is already drawn around it. Standing
    alone on a panel it is only a few strokes with nothing to say it can be
    pressed, so there it is given a face and an edge like any other button.
    */
    property bool outlined: false

    /*
    How far the glyph is turned, in degrees, and it goes there rather than
    appearing there. It is for a mark that stands for a thing that turns: a
    cog over a panel turns as the panel opens, which ties the one to the other
    without a word.
    */
    property real turn: 0

    signal clicked()

    implicitWidth: size
    implicitHeight: size
    opacity: enabled ? 1 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: area.containsMouse ? control.hoverWash
             : control.outlined ? Theme.raised : Qt.alpha(control.hoverWash, 0)
        border.width: control.outlined ? 1 : 0
        border.color: area.containsMouse ? control.hoverTone : Theme.borderStrong

        Behavior on color { ColorAnimation { duration: 100 } }
        Behavior on border.color { ColorAnimation { duration: 100 } }
    }

    Glyph {
        id: mark
        anchors.centerIn: parent
        name: "close"
        tone: area.containsMouse ? control.hoverTone : control.tone
        rotation: control.turn

        Behavior on rotation { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }

    Hint {
        text: control.hint
        visible: control.hint !== "" && area.containsMouse
    }
}
