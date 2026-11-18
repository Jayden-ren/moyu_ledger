import QtQuick
import MoyuLedger

// 统一样式按钮：primary（主行动）/ ghost（描边）/ subtle（无描边）
Rectangle {
    id: btn
    property string text: "按钮"
    property string kind: "ghost"
    signal clicked()

    implicitWidth: label.implicitWidth + (kind === "primary" ? 28 : 24)
    implicitHeight: kind === "primary" ? Theme.primaryBtnH : Theme.controlH
    radius: Theme.radiusBtn
    border.width: kind === "primary" ? 0 : 1
    border.color: Theme.borderStrong
    color: {
        if (btn.kind === "primary")
            return btnArea.containsMouse ? Theme.accentStrong : Theme.accent
        if (btn.kind === "subtle")
            return btnArea.containsMouse ? Theme.bgHover : Theme.bgSubtle
        return btnArea.containsMouse ? Theme.bgHover : Theme.bgSurface
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: btn.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSmall2
        font.weight: btn.kind === "primary" ? Theme.weightSemibold : Theme.weightMedium
        color: btn.kind === "primary" ? Theme.textOnPrimary : Theme.textPrimary
    }

    MouseArea {
        id: btnArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
    }
}