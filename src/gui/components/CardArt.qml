import QtQuick
import QtQuick.Shapes
import Zdl

// The picture at the top of a card. Until there is real artwork it is the
// mark's own tile, which is the same in both shades of the interface.
Item {
    id: art

    property bool lit: false
    property string caption: ""

    // Clipping in Qt Quick is rectangular, so every layer carries the curve
    // itself rather than being cut to it.
    property int rounding: Theme.radius
    property int bottomRounding: 0

    clip: true

    Rectangle {
        anchors.fill: parent
        topLeftRadius: art.rounding
        topRightRadius: art.rounding
        bottomLeftRadius: art.bottomRounding
        bottomRightRadius: art.bottomRounding

        gradient: Gradient {
            GradientStop { position: 0; color: Theme.artTop }
            GradientStop { position: 0.55; color: Theme.artMiddle }
            GradientStop { position: 1; color: Theme.artBottom }
        }
    }

    // Lit from the top left, as the mark is.
    Rectangle {
        anchors.fill: parent
        topLeftRadius: art.rounding
        topRightRadius: art.rounding
        bottomLeftRadius: art.bottomRounding
        bottomRightRadius: art.bottomRounding

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Qt.rgba(1, 1, 1, 0.055) }
            GradientStop { position: 0.7; color: Qt.rgba(1, 1, 1, 0) }
        }
    }

    // The ember, low and left where the mark puts it. A radial fall-off is the
    // one thing a plain rectangle cannot do.
    Shape {
        anchors.fill: parent
        opacity: art.lit ? 1 : 0.6
        preferredRendererType: Shape.CurveRenderer

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        ShapePath {
            strokeWidth: -1

            fillGradient: RadialGradient {
                centerX: art.width * 0.3
                centerY: art.height * 1.05
                centerRadius: Math.max(art.width, art.height) * 0.95
                focalX: centerX
                focalY: centerY

                GradientStop { position: 0; color: Qt.alpha(Theme.ember, 0.38) }
                GradientStop { position: 0.45; color: Qt.alpha(Theme.ember, 0.12) }
                GradientStop { position: 1; color: Qt.alpha(Theme.ember, 0) }
            }

            startX: 0
            startY: art.rounding
            PathArc {
                x: art.rounding
                y: 0
                radiusX: art.rounding
                radiusY: art.rounding
                direction: PathArc.Clockwise
            }
            PathLine { x: art.width - art.rounding; y: 0 }
            PathArc {
                x: art.width
                y: art.rounding
                radiusX: art.rounding
                radiusY: art.rounding
                direction: PathArc.Clockwise
            }
            PathLine { x: art.width; y: art.height - art.bottomRounding }
            PathArc {
                x: art.width - art.bottomRounding
                y: art.height
                radiusX: art.bottomRounding
                radiusY: art.bottomRounding
                direction: PathArc.Clockwise
            }
            PathLine { x: art.bottomRounding; y: art.height }
            PathArc {
                x: 0
                y: art.height - art.bottomRounding
                radiusX: art.bottomRounding
                radiusY: art.bottomRounding
                direction: PathArc.Clockwise
            }
            PathLine { x: 0; y: art.rounding }
        }
    }

    // Held back from full strength so it reads as a placeholder.
    Image {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: art.caption === "" ? 0 : -9

        width: Math.round(Math.min(parent.height * 0.44, 64))
        height: width
        sourceSize.width: 256
        sourceSize.height: 256
        source: "qrc:/qzdl-256.png"
        smooth: true
        opacity: art.lit ? 0.95 : 0.72

        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    Text {
        visible: art.caption !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: parent.width - 24
        text: art.caption
        color: Theme.steel
        opacity: 0.75
        font.pixelSize: Theme.fontTiny
        font.letterSpacing: 1.4
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: art.rounding
        anchors.rightMargin: art.rounding
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }
}
