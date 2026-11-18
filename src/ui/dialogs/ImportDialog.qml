import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import MoyuLedger

// 账单导入：选文件 → 预览（前 30 行）→ 选账户/分类 → 导入（按交易单号去重）
Item {
    id: dialog
    anchors.fill: parent
    visible: false
    z: 900

    signal notifyRequested(string message)

    property string filePath: ""
    property var previewData: ({})
    property int selectedAccountId: -1
    property int selectedCategoryId: -1

    function localPath(url) {
        let text = String(url)
        if (text.startsWith("file:///"))
            text = text.substring(8)
        else if (text.startsWith("file://"))
            text = text.substring(7)
        try { return decodeURIComponent(text) } catch (e) { return text }
    }

    function open() {
        filePath = ""
        previewData = ({})
        selectedAccountId = Ledger.defaultAccountId()
        selectedCategoryId = -1
        visible = true
    }

    function close() { visible = false }

    function applyFile(path) {
        filePath = path
        previewData = Ledger.previewImportCsv(path)
    }

    function doImport() {
        if (!previewData.ok)
            return
        const result = Ledger.importCsv(filePath, selectedAccountId, selectedCategoryId)
        dialog.notifyRequested(result.message)
        if (result.ok)
            close()
    }

    FileDialog {
        id: fileDialog
        title: qsTr("选择账单 CSV")
        nameFilters: [qsTr("CSV 账单 (*.csv)"), qsTr("所有文件 (*)")]
        onAccepted: dialog.applyFile(dialog.localPath(fileDialog.selectedFile))
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent; onClicked: dialog.close() }
    }

    Rectangle {
        width: 760
        height: 600
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.bgSurface
        border.width: 1
        border.color: Theme.borderBase

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.pagePadding
            spacing: Theme.spaceMd

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("导入账单")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH1
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("支持支付宝 / 微信导出的 CSV")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }

            // 文件选择行
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceSm

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.controlH
                    radius: Theme.radiusBtn
                    color: Theme.bgSubtle
                    border.width: 1
                    border.color: Theme.borderBase

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spaceLg
                        anchors.rightMargin: Theme.spaceSm
                        verticalAlignment: Text.AlignVCenter
                        text: dialog.filePath.length > 0 ? dialog.filePath : qsTr("尚未选择文件")
                        elide: Text.ElideMiddle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: dialog.filePath.length > 0 ? Theme.textPrimary : Theme.textTertiary
                    }
                }

                FlatButton {
                    text: qsTr("选择文件")
                    kind: "ghost"
                    onClicked: fileDialog.open()
                }
            }

            // 预览摘要
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                Layout.minimumHeight: 64
                Layout.maximumHeight: 64
                radius: Theme.radiusCard
                color: Theme.bgTint
                border.width: 1
                border.color: Theme.borderBase

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.cardPadding
                    spacing: Theme.spaceXxl

                    Text {
                        Layout.fillWidth: true
                        text: dialog.previewData.ok
                              ? qsTr("识别到 %1 笔：支出 %2 笔 %3，收入 %4 笔 %5")
                                    .arg(dialog.previewData.total)
                                    .arg(dialog.previewData.expenseCount)
                                    .arg(dialog.previewData.expenseText)
                                    .arg(dialog.previewData.incomeCount)
                                    .arg(dialog.previewData.incomeText)
                              : (dialog.previewData.message || qsTr("选择账单文件后显示预览"))
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: dialog.previewData.ok ? Theme.textPrimary : Theme.textSecondary
                    }
                }
            }

            // 预览列表
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spaceSm
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        Text { Layout.preferredWidth: 90; text: qsTr("时间"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 60; text: qsTr("类型"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 100; horizontalAlignment: Text.AlignRight; text: qsTr("金额"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.fillWidth: true; text: qsTr("摘要"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 110; text: qsTr("交易对方"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    }

                    ListView {
                        id: previewList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: dialog.previewData.rows || []

                        delegate: RowLayout {
                            required property var modelData
                            width: previewList.width
                            height: 28
                            spacing: Theme.spaceSm

                            Text { Layout.preferredWidth: 90; text: modelData.dateText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textSecondary }
                            Text { Layout.preferredWidth: 60; text: modelData.kindText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                            Text { Layout.preferredWidth: 100; horizontalAlignment: Text.AlignRight; text: modelData.amountText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; font.features: ({ "tnum": 1 }); color: modelData.kindText === qsTr("收入") ? Theme.income : Theme.textPrimary }
                            Text { Layout.fillWidth: true; text: modelData.note; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; color: Theme.textPrimary }
                            Text { Layout.preferredWidth: 110; text: modelData.payee; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: previewList.count === 0
                            text: dialog.filePath.length === 0 ? qsTr("先选择一个 CSV 账单文件") : qsTr("没有可导入的流水")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textTertiary
                        }
                    }
                }
            }

            // 入账账户
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceSm

                Text {
                    text: qsTr("入账账户")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Row {
                    spacing: Theme.spaceXs

                    Repeater {
                        model: Ledger.accountModel

                        delegate: Rectangle {
                            required property string name
                            required property int accountId
                            width: accLabel.implicitWidth + Theme.spaceXl
                            height: 26
                            radius: Theme.radiusBtn
                            color: dialog.selectedAccountId === accountId ? Theme.accentSoft : Theme.bgSurface
                            border.width: 1
                            border.color: dialog.selectedAccountId === accountId ? Theme.accent : Theme.borderStrong

                            Text {
                                id: accLabel
                                anchors.centerIn: parent
                                text: name
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                color: dialog.selectedAccountId === accountId ? Theme.accent : Theme.textPrimary
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dialog.selectedAccountId = accountId
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // 默认分类（可不选）
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceSm

                Text {
                    text: qsTr("默认分类")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spaceXs

                    Repeater {
                        model: {
                            const list = [{ id: -1, name: qsTr("不指定") }]
                            for (let i = 0; i < Ledger.expenseCategories.length; ++i)
                                list.push(Ledger.expenseCategories[i])
                            return list
                        }

                        delegate: Rectangle {
                            required property var modelData
                            width: catLabel.implicitWidth + Theme.spaceXl
                            height: 26
                            radius: Theme.radiusBtn
                            color: dialog.selectedCategoryId === modelData.id ? Theme.accentSoft : Theme.bgSurface
                            border.width: 1
                            border.color: dialog.selectedCategoryId === modelData.id ? Theme.accent : Theme.borderStrong

                            Text {
                                id: catLabel
                                anchors.centerIn: parent
                                text: modelData.name
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                color: dialog.selectedCategoryId === modelData.id ? Theme.accent : Theme.textPrimary
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dialog.selectedCategoryId = modelData.id
                            }
                        }
                    }
                }
            }

            // 底部
            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: qsTr("提示：按交易单号去重，重复导入不会产生重复流水；失败的交易与退款行会被跳过。")
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Row {
                    spacing: Theme.spaceMd
                    FlatButton { text: qsTr("取消"); kind: "ghost"; onClicked: dialog.close() }
                    FlatButton {
                        text: qsTr("导入")
                        kind: "primary"
                        onClicked: dialog.doImport()
                    }
                }
            }
        }
    }

    Shortcut { sequence: "Escape"; enabled: dialog.visible; onActivated: dialog.close() }
}