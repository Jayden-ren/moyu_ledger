import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 流水列表：按日分组（日期吸顶头）+ 五列行（分类·时间·摘要·账户·金额）
Item {
    id: view
    property alias model: list.model
    property alias currentRow: list.currentIndex
    // 双击某行 -> 打开编辑（单击仍只做选中、右侧显示详情）
    signal rowActivated(int index)

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        spacing: 0
        focus: true
        activeFocusOnTab: true
        currentIndex: -1
        section.property: "dateKey"
        // 模型重置（切月/刷新）后回到顶部，避免停在旧位置
        Connections {
            target: list.model
            function onModelReset() {
                list.currentIndex = -1
                Qt.callLater(list.positionViewAtBeginning)
            }
        }
        section.criteria: ViewSection.FullString
        section.delegate: Rectangle {
            id: sectionHeader
            required property string section
            width: ListView.view.width
            height: Theme.groupHeaderH
            color: Theme.bgSubtle

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spaceLg + 2
                anchors.rightMargin: Theme.spaceLg
                spacing: 0

                Text {
                    text: view.model ? view.model.dayLabel(sectionHeader.section) : ""
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    font.weight: Theme.weightMedium
                    color: Theme.textSecondary
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: {
                        // 读取一次隐私开关：切换「隐藏金额」时这个绑定会重新求值
                        const privacy = Ledger.privacyMode
                        return (view.model && privacy !== undefined)
                               ? view.model.dayTotalText(sectionHeader.section) : ""
                    }
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }

            Rectangle {
                height: 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: Theme.borderDivider
            }
        }

        delegate: Rectangle {
            id: row
            required property int index
            required property string categoryBadge
            required property string categoryName
            required property string categoryColor
            required property string occurredText
            required property string occurredFullText
            required property string note
            required property string accountName
            required property string amountText
            required property bool isExpense
            required property string kind
            required property string toAccountName

            width: ListView.view.width
            height: Theme.rowH
            readonly property bool selected: ListView.view.currentIndex === row.index
            color: row.selected ? Theme.bgSelection : (rowArea.containsMouse ? Theme.bgHover : "transparent")

            Rectangle {
                visible: row.selected || rowArea.containsMouse
                width: Theme.selectedBarW
                height: parent.height
                color: Theme.accent
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spaceLg + 2
                anchors.rightMargin: Theme.spaceLg
                spacing: 0

                // 分类：徽标 + 名称
                RowLayout {
                    Layout.preferredWidth: Theme.colCategoryW
                    spacing: Theme.spaceS

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 11
                        color: row.kind === "transfer"
                               ? Theme.accent
                               : (row.categoryColor.length > 0 ? row.categoryColor : Theme.textTertiary)
                        Text {
                            anchors.centerIn: parent
                            text: row.kind === "transfer" ? "转" : row.categoryBadge
                            color: Theme.textOnAccent
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            font.weight: Theme.weightMedium
                        }
                    }
                    Text {
                        text: row.kind === "transfer" ? qsTr("转账") : row.categoryName
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        color: Theme.textSecondary
                    }
                }

                Text {
                    Layout.preferredWidth: Theme.colTimeW
                    text: row.occurredFullText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Text {
                    Layout.fillWidth: true
                    text: row.kind === "transfer"
                          ? row.accountName + " → " + row.toAccountName
                            + (row.note.length > 0 ? "  ·  " + row.note : "")
                          : row.note
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    color: Theme.textPrimary
                }

                Text {
                    Layout.preferredWidth: Theme.colAccountW
                    text: row.kind === "transfer" ? "" : row.accountName
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                }

                Text {
                    Layout.preferredWidth: Theme.colAmountW
                    horizontalAlignment: Text.AlignRight
                    text: row.kind === "transfer" ? row.amountText.replace("-", "") : row.amountText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Theme.weightMedium
                    font.features: ({ "tnum": 1 })
                    color: row.kind === "transfer"
                           ? Theme.textSecondary
                           : (row.isExpense ? Theme.expense : Theme.income)
                }
            }

            Rectangle {
                height: 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: Theme.borderHairline
            }

            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    // 注意：委托里的 ListView.view 在 Qt 6.11 这里是 null，
                    // 必须用根 Item 的别名，否则选中状态不生效、右侧详情空白
                    view.currentRow = row.index
                }
                onDoubleClicked: {
                    view.currentRow = row.index
                    view.rowActivated(row.index)
                }
            }
        }
    }
}