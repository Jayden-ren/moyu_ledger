import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 工具条 56px：月份切换 / 搜索 / 筛选 / 命令面板入口 / 导出 / 记一笔
Rectangle {
    id: bar
    height: Theme.toolBarH
    color: Theme.bgSurface
    signal addRequested()
    signal commandRequested()
    signal searchChanged(string text)
    signal filterRequested()
    signal exportRequested()
    signal importRequested()
    readonly property alias searchField: searchField_impl

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pagePadding
        anchors.rightMargin: Theme.pagePadding
        spacing: Theme.spaceMd

        // ---- 月份切换 ----
        Rectangle {
            Layout.preferredWidth: Theme.monthSwitchW
            Layout.preferredHeight: Theme.controlH
            radius: Theme.radiusBtn
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderStrong

            Row {
                anchors.fill: parent

                Item {
                    width: 28
                    height: parent.height
                    Text {
                        anchors.centerIn: parent
                        text: "‹"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        color: prevArea.containsMouse ? Theme.accent : Theme.textSecondary
                    }
                    MouseArea {
                        id: prevArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: AppContext.previousMonth()
                    }
                }

                Text {
                    width: Theme.monthSwitchW - 56 - 2
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: AppContext.currentMonth
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    font.weight: Theme.weightMedium
                    color: Theme.textPrimary
                }

                Item {
                    width: 28
                    height: parent.height
                    Text {
                        anchors.centerIn: parent
                        text: "›"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        color: nextArea.containsMouse ? Theme.accent : Theme.textSecondary
                    }
                    MouseArea {
                        id: nextArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: AppContext.nextMonth()
                    }
                }
            }
        }

        // ---- 搜索 ----
        Rectangle {
            Layout.preferredWidth: Theme.searchW
            Layout.preferredHeight: Theme.controlH
            radius: Theme.radiusBtn
            color: Theme.bgSurface
            border.width: 1
            border.color: searchField_impl.activeFocus ? Theme.accent : Theme.borderStrong

            Timer {
                id: searchDebounce
                interval: 250
                onTriggered: bar.searchChanged(searchField_impl.text)
            }

            TextInput {
                id: searchField_impl
                anchors.fill: parent
                anchors.leftMargin: Theme.spaceLg
                anchors.rightMargin: Theme.spaceSm
                verticalAlignment: TextInput.AlignVCenter
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall2
                color: Theme.textPrimary
                selectionColor: Theme.accentSoftStrong
                selectedTextColor: Theme.textPrimary
                clip: true

                onTextChanged: searchDebounce.restart()

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: searchField_impl.text.length === 0
                    text: qsTr("搜索摘要、分类、账户、金额")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textTertiary
                }
            }

            // 清除
            Item {
                visible: searchField_impl.text.length > 0
                width: 22
                height: parent.height
                anchors.right: parent.right
                anchors.rightMargin: 4

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: clearArea.containsMouse ? Theme.textPrimary : Theme.textTertiary
                }

                MouseArea {
                    id: clearArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        searchField_impl.text = ""
                        bar.searchChanged("")
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: filterLabel.implicitWidth + Theme.spaceXxl
            Layout.preferredHeight: Theme.controlH
            radius: Theme.radiusBtn
            color: (Ledger.filterActive || filterArea.containsMouse) ? Theme.bgHover : Theme.bgSurface
            border.width: 1
            border.color: Ledger.filterActive ? Theme.accent : Theme.borderStrong

            Text {
                id: filterLabel
                anchors.centerIn: parent
                text: qsTr("筛选")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall2
                font.weight: Ledger.filterActive ? Theme.weightSemibold : Theme.weightMedium
                color: Ledger.filterActive ? Theme.accent : Theme.textPrimary
            }

            MouseArea {
                id: filterArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: bar.filterRequested()
            }
        }
        FlatButton { text: qsTr("新增"); kind: "subtle"; visible: false }

        Item { Layout.fillWidth: true }

        // ---- 金额隐藏开关（默认关闭）----
        Rectangle {
            id: privacyToggle
            Layout.preferredHeight: Theme.controlH
            Layout.preferredWidth: 34
            radius: Theme.radiusBtn
            color: Ledger.privacyMode ? Theme.accentSoft
                                     : (privacyMouse.containsMouse ? Theme.bgHover : Theme.bgSurface)
            border.width: 1
            border.color: Ledger.privacyMode ? Theme.accent : Theme.borderStrong

            Text {
                anchors.centerIn: parent
                text: Ledger.privacyMode ? "🙈" : "👁"
                font.pixelSize: 14
            }

            MouseArea {
                id: privacyMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Ledger.setPrivacyMode(!Ledger.privacyMode)
            }
        }

        // ---- 命令面板入口 ----
        Rectangle {
            Layout.preferredHeight: Theme.controlH
            Layout.preferredWidth: commandRow.implicitWidth + Theme.spaceXxl
            radius: Theme.radiusBtn
            color: commandArea.containsMouse ? Theme.bgHover : Theme.bgSurface
            border.width: 1
            border.color: Theme.borderStrong

            RowLayout {
                id: commandRow
                anchors.centerIn: parent
                spacing: Theme.spaceS

                Text {
                    text: qsTr("命令")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                }
                KeyCap { text: "Ctrl K" }
            }

            MouseArea {
                id: commandArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: bar.commandRequested()
            }
        }

        FlatButton {
            text: qsTr("导出")
            kind: "ghost"
            onClicked: bar.exportRequested()
        }

        FlatButton {
            text: qsTr("导入")
            kind: "ghost"
            onClicked: bar.importRequested()
        }

        FlatButton {
            text: qsTr("＋ 记一笔")
            kind: "primary"
            onClicked: bar.addRequested()
        }
    }

    Rectangle {
        height: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: Theme.borderBase
    }
}