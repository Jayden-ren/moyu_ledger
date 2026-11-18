import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 筛选面板：分类 + 账户（单选，再点取消）；直接调用 Ledger 的筛选接口
Rectangle {
    id: panel
    width: 360
    visible: false
    radius: Theme.radiusCard
    color: Theme.bgSurface
    border.width: 1
    border.color: Theme.borderBase
    z: 800

    implicitHeight: content.implicitHeight + Theme.pagePadding * 2

    function toggle() {
        visible = !visible
    }

    // 点击面板外关闭
    onVisibleChanged: if (!visible) Qt.callLater(() => {})

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.pagePadding
        spacing: Theme.spaceSm

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("筛选流水")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                font.weight: Theme.weightSemibold
                color: Theme.textPrimary
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("清除全部")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: clearArea.containsMouse ? Theme.dangerFg : Theme.textTertiary
                MouseArea {
                    id: clearArea
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        Ledger.clearFilter()
                        panel.visible = false
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: Ledger.filterActive
            text: Ledger.filterSummary + qsTr("（") + Ledger.filteredCount + qsTr(" 笔）")
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.accent
        }

        Text {
            text: qsTr("支出分类")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }

        Flow {
            Layout.fillWidth: true
            spacing: Theme.spaceS

            Repeater {
                model: Ledger.expenseCategories

                delegate: Rectangle {
                    required property var modelData
                    property bool selected: Ledger.filterSummary.indexOf(modelData.name) >= 0
                    width: chipText.implicitWidth + Theme.spaceXl
                    height: 26
                    radius: Theme.radiusBtn
                    color: selected ? Theme.accentSoft : Theme.bgSurface
                    border.width: 1
                    border.color: selected ? Theme.accent : Theme.borderStrong

                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: modelData.name
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: selected ? Theme.accent : Theme.textPrimary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (Ledger.filterSummary.indexOf(modelData.name) >= 0)
                                Ledger.setCategoryFilter(-1)
                            else
                                Ledger.setCategoryFilter(modelData.id)
                        }
                    }
                }
            }
        }

        Text {
            text: qsTr("账户")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }

        Flow {
            Layout.fillWidth: true
            spacing: Theme.spaceS

            Repeater {
                model: Ledger.accountModel

                delegate: Rectangle {
                    required property string name
                    required property int accountId
                    property bool selected: Ledger.filterSummary.indexOf(name) >= 0
                    width: accountText.implicitWidth + Theme.spaceXl
                    height: 26
                    radius: Theme.radiusBtn
                    color: selected ? Theme.accentSoft : Theme.bgSurface
                    border.width: 1
                    border.color: selected ? Theme.accent : Theme.borderStrong

                    Text {
                        id: accountText
                        anchors.centerIn: parent
                        text: name
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: selected ? Theme.accent : Theme.textPrimary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (Ledger.filterSummary.indexOf(name) >= 0)
                                Ledger.setAccountFilter(-1)
                            else
                                Ledger.setAccountFilter(accountId)
                        }
                    }
                }
            }
        }
    }
}