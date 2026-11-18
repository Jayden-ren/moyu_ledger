import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 设置页：外观、数据、关于与快捷键
Item {
    id: page
    signal notifyRequested(string message)
    signal addRecurringRequested()
    signal manageCategoriesRequested()
    signal editRecurringRequested(var rule)

    readonly property var shortcuts: [
        { keys: "Ctrl N", text: qsTr("记一笔") },
        { keys: "Ctrl K", text: qsTr("命令面板") },
        { keys: "Ctrl F", text: qsTr("搜索流水") },
        { keys: "Ctrl 1-7", text: qsTr("切换页面") },
        { keys: "Ctrl T", text: qsTr("回到本月") },
        { keys: "Ctrl E", text: qsTr("导出本月 CSV") },
        { keys: "Ctrl Z / Shift+Z", text: qsTr("撤销 / 重做") },
        { keys: "Ctrl Shift L", text: qsTr("切换暗色主题") },
        { keys: "↑ ↓", text: qsTr("选择流水") },
        { keys: "Enter", text: qsTr("查看详情") }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.cardGap

        Text {
            text: qsTr("设置")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontH2
            font.weight: Theme.weightSemibold
            color: Theme.textPrimary
        }

        // 外观
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            Layout.minimumHeight: 96
            Layout.maximumHeight: 96
            radius: Theme.radiusCard
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderBase

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: Theme.spaceMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: qsTr("暗色主题")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightMedium
                        color: Theme.textPrimary
                    }
                    Text {
                        text: qsTr("立即生效；快捷键 Ctrl+Shift+L")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }

                Rectangle {
                    id: themeSwitch
                    width: 44
                    height: 24
                    radius: 12
                    color: AppContext.darkTheme ? Theme.accent : Theme.borderStrong

                    Rectangle {
                        width: 18
                        height: 18
                        radius: 9
                        y: 3
                        x: AppContext.darkTheme ? 23 : 3
                        color: Theme.textOnAccent

                        Behavior on x { NumberAnimation { duration: 120 } }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            AppContext.toggleDarkTheme()
                            page.notifyRequested(AppContext.darkTheme ? qsTr("已切换到暗色主题")
                                                                     : qsTr("已切换到浅色主题"))
                        }
                    }
                }
            }
        }

        // 数据
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            Layout.minimumHeight: 168
            Layout.maximumHeight: 168
            radius: Theme.radiusCard
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderBase

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: Theme.spaceS

                Text {
                    text: qsTr("数据")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spaceSm

                    Text {
                        Layout.fillWidth: true
                        text: Ledger.databasePath()
                        elide: Text.ElideMiddle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textSecondary
                    }

                    FlatButton {
                        text: qsTr("打开所在文件夹")
                        kind: "ghost"
                        onClicked: Ledger.openDataFolder()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spaceXxl

                    Text {
                        text: qsTr("本月 ") + Ledger.transactionCount + qsTr(" 笔")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textSecondary
                    }
                    Text {
                        text: qsTr("累计 ") + Ledger.totalTransactionCount() + qsTr(" 笔")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textSecondary
                    }
                    Text {
                        text: qsTr("账户 ") + Ledger.accountModel.count + qsTr(" 个")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textSecondary
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spaceSm

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("上次备份：") + Ledger.lastBackupText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    FlatButton {
                        text: qsTr("立即备份")
                        kind: "ghost"
                        onClicked: page.notifyRequested(Ledger.backupNow().message)
                    }

                    FlatButton {
                        text: qsTr("导出全量 JSON")
                        kind: "ghost"
                        onClicked: page.notifyRequested(Ledger.exportAllJson("").message)
                    }

                    FlatButton {
                        text: qsTr("分类管理")
                        kind: "ghost"
                        onClicked: page.manageCategoriesRequested()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("数据全部存放在本地 SQLite 文件中，不上传云端；每次启动自动备份，保留最近 7 份（与应用同目录的 backups 文件夹）。")
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }
        }

        // 定期账单
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 186
            Layout.minimumHeight: 186
            Layout.maximumHeight: 186
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
                        text: qsTr("定期账单")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: qsTr("每月自动生成一笔，重复执行不会重复记账")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                    FlatButton {
                        text: qsTr("＋ 新增")
                        kind: "ghost"
                        onClicked: page.addRecurringRequested()
                    }
                }

                ListView {
                    id: recurringList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Ledger.recurringRules
                    spacing: 1

                    delegate: RowLayout {
                        required property var modelData
                        width: recurringList.width
                        spacing: Theme.spaceSm

                        Rectangle {
                            width: 44
                            height: 22
                            radius: Theme.radiusBtn
                            color: modelData.kind === "income" ? Theme.incomeSoft : Theme.expenseSoft

                            Text {
                                anchors.centerIn: parent
                                text: modelData.kindText
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: modelData.kind === "income" ? Theme.income : Theme.expense
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: (modelData.note.length > 0 ? modelData.note : qsTr("（未命名）"))
                                  + " · 每月 " + modelData.dayOfMonth + " 号"
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textPrimary
                        }

                        Text {
                            Layout.preferredWidth: 110
                            text: modelData.accountName + (modelData.categoryName.length > 0 ? " · " + modelData.categoryName : "")
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        Text {
                            Layout.preferredWidth: 96
                            horizontalAlignment: Text.AlignRight
                            text: modelData.amountText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                        }

                        FlatButton {
                            text: modelData.enabled ? qsTr("暂停") : qsTr("启用")
                            kind: "ghost"
                            onClicked: {
                                const result = Ledger.setRecurringEnabled(modelData.id, !modelData.enabled)
                                page.notifyRequested(result.message)
                            }
                        }

                        FlatButton {
                            text: qsTr("删除")
                            kind: "ghost"
                            onClicked: {
                                const result = Ledger.removeRecurring(modelData.id)
                                page.notifyRequested(result.message)
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: recurringList.count === 0
                        text: qsTr("还没有定期账单，点「＋ 新增」添加房租、订阅等固定支出")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textTertiary
                    }
                }
            }
        }

        // 快捷键 + 关于
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.cardGap

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
                    spacing: Theme.spaceS

                    Text {
                        text: qsTr("快捷键")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        columns: 2
                        columnSpacing: Theme.spaceXxl
                        rowSpacing: Theme.spaceXs

                        Repeater {
                            model: page.shortcuts

                            delegate: RowLayout {
                                required property var modelData
                                spacing: Theme.spaceSm

                                KeyCap { text: modelData.keys }
                                Text {
                                    text: modelData.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    color: Theme.textSecondary
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.cardPadding
                    spacing: Theme.spaceS

                    Text {
                        text: qsTr("关于")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }

                    Text {
                        text: qsTr("摸鱼记账 v%1").arg(AppContext.appVersion)
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Qt 6.11 · C++20 · Qt Quick · SQLite\n本地单机记账，无联网、无账号。")
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}