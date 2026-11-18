import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 右详情栏 352px：空态 / 选中流水详情（金额、分类、账户、日期、备注、标签、操作）
Rectangle {
    id: pane
    width: Theme.detailW
    color: Theme.bgSurface

    property var detail: ({})
    property bool confirmDelete: false
    readonly property bool hasDetail: detail && detail.transactionId !== undefined

    signal editRequested(var detail)
    signal deleteRequested(int id)
    signal duplicateRequested()

    onDetailChanged: confirmDelete = false

    Rectangle {
        width: 1
        height: parent.height
        anchors.left: parent.left
        color: Theme.borderBase
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: Theme.bgSurface   // Rectangle 默认是白色，暗色主题下必须显式指定

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.pagePadding
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("交易详情")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                font.weight: Theme.weightSemibold
                color: Theme.textPrimary
            }

            Rectangle {
                height: 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: Theme.borderHairline
            }
        }

        // ---- 空态 ----
        Item {
            visible: !pane.hasDetail
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                anchors.centerIn: parent
                spacing: Theme.spaceS

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("选中一笔流水")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("分类、账户、备注与标签会显示在这里")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }
        }

        // ---- 详情 ----
        ColumnLayout {
            visible: pane.hasDetail
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.pagePadding
            spacing: Theme.spaceMd

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd

                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    color: pane.hasDetail ? pane.detail.categoryColor : Theme.textTertiary
                    Text {
                        anchors.centerIn: parent
                        text: pane.hasDetail ? pane.detail.categoryBadge : ""
                        color: Theme.textOnAccent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        font.weight: Theme.weightMedium
                    }
                }

                ColumnLayout {
                    spacing: 0
                    Text {
                        text: pane.hasDetail ? pane.detail.categoryName : ""
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody2
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: pane.hasDetail ? pane.detail.kindText : ""
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Text {
                Layout.fillWidth: true
                text: pane.hasDetail ? pane.detail.amountText : ""
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontAmount
                font.weight: Theme.weightSemibold
                font.features: ({ "tnum": 1 })
                color: pane.hasDetail && pane.detail.isExpense ? Theme.expense : Theme.income
            }

            Text {
                Layout.fillWidth: true
                text: pane.hasDetail ? pane.detail.note : ""
                wrapMode: Text.Wrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody2
                font.weight: Theme.weightMedium
                color: Theme.textPrimary
            }

            Text {
                Layout.fillWidth: true
                text: pane.hasDetail ? pane.detail.dateText : ""
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: Theme.spaceXs
                color: Theme.borderDivider
            }

            // 注意：ColumnLayout 里不能放 Repeater（会与布局互相触发），这里显式写 4 行
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.detailKvRowH
                spacing: Theme.spaceMd
                Text {
                    text: qsTr("账户")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
                Item { Layout.fillWidth: true }
                Text {
                    Layout.maximumWidth: 220
                    text: pane.hasDetail ? pane.detail.accountName : ""
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.detailKvRowH
                spacing: Theme.spaceMd
                Text {
                    text: qsTr("分类")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
                Item { Layout.fillWidth: true }
                Text {
                    Layout.maximumWidth: 220
                    text: pane.hasDetail ? pane.detail.categoryName : ""
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.detailKvRowH
                spacing: Theme.spaceMd
                Text {
                    text: qsTr("备注")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
                Item { Layout.fillWidth: true }
                Text {
                    Layout.maximumWidth: 220
                    text: pane.hasDetail ? pane.detail.note : ""
                    elide: Text.ElideRight

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd
                visible: pane.hasDetail && pane.detail.kind === "transfer"

                Text {
                    Layout.preferredWidth: 56
                    text: qsTr("转账")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Text {
                    Layout.fillWidth: true
                    text: pane.hasDetail
                          ? pane.detail.accountName + "  →  " + (pane.detail.toAccountName || "")
                          : ""
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.accent
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd
                visible: pane.hasDetail && (pane.detail.tagsText || "").length > 0

                Text {
                    Layout.preferredWidth: 56
                    text: qsTr("标签")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Text {
                    Layout.fillWidth: true
                    text: pane.hasDetail ? (pane.detail.tagsText || "") : ""
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                }
            }

                    horizontalAlignment: Text.AlignRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.detailKvRowH
                spacing: Theme.spaceMd
                Text {
                    text: qsTr("标签")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
                Item { Layout.fillWidth: true }
                Text {
                    Layout.maximumWidth: 220
                    text: qsTr("暂未使用")
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                }
            }

            Item { Layout.fillHeight: true }

            FlatButton {
                Layout.fillWidth: true
                text: qsTr("编辑这笔")
                kind: "primary"
                onClicked: pane.editRequested(pane.detail)
            }

            FlatButton {
                Layout.fillWidth: true
                text: qsTr("记一笔类似")
                kind: "subtle"
                onClicked: pane.duplicateRequested()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd

                FlatButton {
                    Layout.fillWidth: true
                    text: qsTr("复制")
                    kind: "ghost"
                    onClicked: pane.duplicateRequested()
                }

                FlatButton {
                    Layout.fillWidth: true
                    text: pane.confirmDelete ? qsTr("确认删除") : qsTr("删除")
                    kind: "ghost"
                    onClicked: {
                        if (pane.confirmDelete)
                            pane.deleteRequested(pane.detail.transactionId)
                        else
                            pane.confirmDelete = true
                    }
                }
            }
        }
    }
}