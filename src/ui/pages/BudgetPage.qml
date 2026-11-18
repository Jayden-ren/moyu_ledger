import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 预算页：总预算编辑 + 分类预算（行内编辑，编辑完成即保存）
Item {
    id: page
    signal notifyRequested(string message)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.cardGap

        // 头部
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceMd

            Text {
                text: qsTr("预算")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontH2
                font.weight: Theme.weightSemibold
                color: Theme.textPrimary
            }

            Rectangle { width: 1; height: 18; color: Theme.borderBase }

            Text {
                text: AppContext.currentMonth
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody2
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("编辑金额后按 Enter 保存")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            FlatButton {
                text: qsTr("从上月复制")
                kind: "ghost"
                onClicked: page.notifyRequested(Ledger.copyBudgetFromPreviousMonth().message)
            }

            FlatButton {
                text: qsTr("复制到下一月")
                kind: "ghost"
                onClicked: page.notifyRequested(Ledger.copyBudgetToNextMonth().message)
            }
        }

        // 总预算卡
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 118
            Layout.minimumHeight: 118
            Layout.maximumHeight: 118
            radius: Theme.radiusCard
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderBase

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: Theme.spaceXxl

                ColumnLayout {
                    Layout.preferredWidth: 220
                    spacing: 4

                    Text {
                        text: qsTr("本月总预算")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Rectangle {
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: Theme.controlH
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: totalInput.activeFocus ? Theme.accent : Theme.borderStrong

                        TextInput {
                            id: totalInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceLg
                            anchors.rightMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            text: Ledger.hasBudget ? Ledger.budgetLimitText.replace("¥", "").replace(",", "") : ""
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                visible: totalInput.text.length === 0
                                text: qsTr("未设置，例如 6500")
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                color: Theme.textTertiary
                            }

                            onEditingFinished: {
                                const result = Ledger.setTotalBudget(text)
                                page.notifyRequested(result.message)
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("已用 ") + (Ledger.hasBudget ? Ledger.budgetUsedText : "—")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            font.weight: Theme.weightMedium
                            color: Ledger.budgetNearLimit ? Theme.warnFg : Theme.textSecondary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("剩余 ") + (Ledger.hasBudget ? Ledger.budgetRemainText : "—")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textSecondary
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: Theme.chartTrack

                        Rectangle {
                            width: parent.width * Math.max(0, Math.min(1, Ledger.budgetRatio))
                            height: parent.height
                            radius: 4
                            color: Ledger.budgetNearLimit ? Theme.warnSolid : Theme.accent
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: Ledger.hasBudget
                              ? qsTr("本月已支出 %1，额度 %2").arg(Ledger.expenseText).arg(Ledger.budgetLimitText)
                              : qsTr("尚未设置总预算：填一个金额后按 Enter 即可")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }
            }
        }

        // 分类预算超配提示
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Ledger.budgetOverAllocated ? 38 : 0
            Layout.minimumHeight: Ledger.budgetOverAllocated ? 38 : 0
            Layout.maximumHeight: Ledger.budgetOverAllocated ? 38 : 0
            visible: Ledger.budgetOverAllocated
            radius: Theme.radiusCard
            color: Theme.warnBg
            border.width: 1
            border.color: Theme.warnBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.cardPadding
                anchors.rightMargin: Theme.cardPadding
                spacing: Theme.spaceSm

                Text {
                    text: "!"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    font.weight: Theme.weightBold
                    color: Theme.warnFg
                }

                Text {
                    Layout.fillWidth: true
                    text: Ledger.budgetWarningText
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.warnFg
                }

                Text {
                    text: qsTr("分类合计 ") + Ledger.budgetCategorySumText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.warnFg
                }
            }
        }

        // 分类预算
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusCard
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderBase

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: Theme.spaceXs

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("分类预算")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: qsTr("留空并回车可清除该分类预算")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }

                // 列头
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.spaceXs
                    spacing: 0

                    Text { Layout.preferredWidth: 150; text: qsTr("分类"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Text { Layout.preferredWidth: 110; text: qsTr("预算"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Text { Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight; text: qsTr("已花"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Item { Layout.fillWidth: true }
                    Text { Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight; text: qsTr("剩余 / 超支"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                }

                ListView {
                    id: budgetList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 1
                    model: Ledger.budgetRows

                    delegate: Rectangle {
                        required property var modelData

                        width: budgetList.width
                        height: 44
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            // 分类
                            RowLayout {
                                Layout.preferredWidth: 150
                                spacing: Theme.spaceS

                                Rectangle {
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: modelData.color
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.badge
                                        color: Theme.textOnAccent
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontMicro3
                                        font.weight: Theme.weightMedium
                                    }
                                }

                                Text {
                                    text: modelData.name
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    color: Theme.textPrimary
                                }
                            }

                            // 预算输入
                            Rectangle {
                                Layout.preferredWidth: 100
                                Layout.preferredHeight: 28
                                radius: Theme.radiusBtn
                                color: Theme.bgSurface
                                border.width: 1
                                border.color: budgetInput.activeFocus ? Theme.accent : Theme.borderStrong

                                TextInput {
                                    id: budgetInput
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spaceSm
                                    anchors.rightMargin: Theme.spaceSm
                                    verticalAlignment: TextInput.AlignVCenter
                                    text: modelData.budgetInput
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                    selectionColor: Theme.accentSoftStrong

                                    Text {
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        visible: budgetInput.text.length === 0
                                        text: qsTr("未设置")
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSmall2
                                        color: Theme.textTertiary
                                    }

                                    onEditingFinished: {
                                        const result = Ledger.setCategoryBudget(modelData.categoryId, text)
                                        page.notifyRequested(result.message)
                                    }
                                }
                            }

                            // 已花
                            Text {
                                Layout.preferredWidth: 110
                                horizontalAlignment: Text.AlignRight
                                text: modelData.spentText
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                font.features: ({ "tnum": 1 })
                                color: Theme.textPrimary
                            }

                            // 进度条
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.spaceXxl
                                Layout.rightMargin: Theme.spaceXxl
                                height: 6
                                radius: 3
                                color: Theme.chartTrack

                                Rectangle {
                                    width: parent.width * Math.max(0, Math.min(1, modelData.ratio))
                                    height: parent.height
                                    radius: 3
                                    color: modelData.overLimit
                                           ? Theme.dangerFg
                                           : (modelData.ratio >= 0.85 ? Theme.warnSolid : Theme.accent)
                                }
                            }

                            // 剩余 / 超支
                            Text {
                                Layout.preferredWidth: 130
                                horizontalAlignment: Text.AlignRight
                                text: modelData.hasBudget
                                      ? (modelData.overLimit
                                         ? qsTr("超支 ") + modelData.remainText.replace("-", "").replace("¥", "¥")
                                         : qsTr("剩余 ") + modelData.remainText)
                                      : "—"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                color: modelData.overLimit ? Theme.dangerFg
                                                           : (modelData.hasBudget ? Theme.textSecondary : Theme.textDisabled)
                            }
                        }

                        Rectangle {
                            height: 1
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            color: Theme.borderHairline
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("提示：分类预算用于单独盯住某一类支出；总预算控制整体上限。超支的行会标红。")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }
        }
    }
}