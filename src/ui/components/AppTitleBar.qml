import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 自绘标题栏 40px：品牌 + 账本名 + 窗口按钮；空白区可拖动、双击最大化
Rectangle {
    id: bar
    height: Theme.titleBarH
    color: Theme.bgSurface

    // 宿主窗口由 Main.qml 注入（DragHandler 不是 Item，不能用 Window.window 附件属性）
    property var hostWindow: null

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: Theme.windowBtnW * 3
        onDoubleClicked: {
            if (bar.hostWindow && bar.hostWindow.visibility === Window.Maximized)
                bar.hostWindow.showNormal()
            else if (bar.hostWindow)
                bar.hostWindow.showMaximized()
        }
    }

    DragHandler {
        target: null
        onActiveChanged: if (active && bar.hostWindow) bar.hostWindow.startSystemMove()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceMd + 2
        spacing: Theme.spaceSm

        Rectangle {
            width: 18
            height: 18
            radius: Theme.radiusChip
            color: Theme.accent
            Text {
                anchors.centerIn: parent
                text: qsTr("账")
                color: Theme.textInverse
                font.family: Theme.fontFamily
                font.pixelSize: 10
                font.weight: Theme.weightBold
            }
        }

        Text {
            text: qsTr("摸鱼记账")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall2
            font.weight: Theme.weightSemibold
            color: Theme.textPrimary
        }

        Rectangle {
            width: 1
            height: 12
            color: Theme.borderBase
            Layout.alignment: Qt.AlignVCenter
        }

        // 当前账本（可点击切换）
        Rectangle {
            id: ledgerChip
            Layout.alignment: Qt.AlignVCenter
            width: chipRow.implicitWidth + Theme.spaceMd
            height: 22
            radius: Theme.radiusChip
            color: ledgerMouse.containsMouse ? Theme.bgHover : "transparent"

            Row {
                id: chipRow
                anchors.centerIn: parent
                spacing: 4

                Text {
                    text: Ledger.currentLedgerName
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.weight: Theme.weightMedium
                    color: ledgerMouse.containsMouse ? Theme.accent : Theme.textSecondary
                }

                Text {
                    text: "▾"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }

            MouseArea {
                id: ledgerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: ledgerSwitcher.visible ? ledgerSwitcher.close() : ledgerSwitcher.open()
            }
        }

        Text {
            text: AppContext.year + qsTr(" 年")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            color: Theme.textTertiary
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        Row {
            spacing: 0
            WindowButton {
                kind: "min"
                onClicked: if (bar.hostWindow) bar.hostWindow.showMinimized()
            }
            WindowButton {
                kind: "max"
                onClicked: {
                    if (!bar.hostWindow)
                        return
                    if (bar.hostWindow.visibility === Window.Maximized)
                        bar.hostWindow.showNormal()
                    else
                        bar.hostWindow.showMaximized()
                }
            }
            WindowButton {
                kind: "close"
                onClicked: if (bar.hostWindow) bar.hostWindow.close()
            }
        }
    }

    Rectangle {
        height: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: Theme.borderBase
    }

    // 账本切换浮层（不能放进 RowLayout，否则会被布局拉伸）
    LedgerSwitcher {
        id: ledgerSwitcher
        x: Theme.spaceMd + 70
        y: bar.height + 6
        onClosed: {}
    }
}