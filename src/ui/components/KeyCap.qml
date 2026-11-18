import QtQuick
import MoyuLedger

// 键位提示小胶囊：状态条与命令面板共用
Rectangle {
    id: keyCap
    property string text: "Ctrl"

    implicitWidth: label.implicitWidth + Theme.spaceSm
    implicitHeight: 16
    radius: Theme.radiusChip
    color: Theme.bgSurface
    border.width: 1
    border.color: Theme.borderBase

    Text {
        id: label
        anchors.centerIn: parent
        text: keyCap.text
        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontMicro2
        color: Theme.textSecondary
    }
}