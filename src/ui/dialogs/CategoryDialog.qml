import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 分类管理：新增 / 改名 / 改色 / 归档（内置分类不可删除，只能归档）
Item {
    id: dialog
    anchors.fill: parent
    visible: false
    z: 900

    signal notifyRequested(string message)

    property int editingId: -1
    property string kind: "expense"
    property string selectedColor: "#64748B"
    property string errorText: ""

    readonly property var colorChoices: ["#2563EB", "#F97316", "#EF4444", "#EC4899",
                                    "#8B5CF6", "#06B6D4", "#10B981", "#64748B"]

    function open() {
        resetForm()
        visible = true
    }

    function close() { visible = false }

    function resetForm() {
        editingId = -1
        kind = "expense"
        selectedColor = colorChoices[0]
        nameInput.text = ""
        errorText = ""
    }

    function beginEdit(item) {
        editingId = item.id
        kind = item.kind
        selectedColor = item.color && item.color.length > 0 ? item.color : colorChoices[0]
        nameInput.text = item.name
        errorText = ""
        nameInput.forceActiveFocus()
    }

    function submit() {
        const result = editingId > 0
            ? Ledger.updateCategory(editingId, nameInput.text, selectedColor)
            : Ledger.addCategory(kind, nameInput.text, selectedColor)
        if (result.ok) {
            dialog.notifyRequested(result.message)
            resetForm()
        } else {
            errorText = result.message
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent; onClicked: dialog.close() }
    }

    Rectangle {
        width: 660
        height: 580
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
                    text: qsTr("分类管理")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH1
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("归档后不再出现在记账选择里，历史流水保留")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }

            // 表单
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                Layout.minimumHeight: 132
                radius: Theme.radiusCard
                color: Theme.bgSubtle
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spaceMd
                    spacing: Theme.spaceXs

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs

                        Repeater {
                            model: [{ key: "expense", label: qsTr("支出") }, { key: "income", label: qsTr("收入") }]

                            delegate: Rectangle {
                                required property var modelData
                                width: kindLabel.implicitWidth + Theme.spaceLg
                                height: 24
                                radius: Theme.radiusBtn
                                color: dialog.kind === modelData.key ? Theme.accentSoft : Theme.bgSurface
                                border.width: 1
                                border.color: dialog.kind === modelData.key ? Theme.accent : Theme.borderStrong

                                Text {
                                    id: kindLabel
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: dialog.kind === modelData.key ? Theme.accent : Theme.textPrimary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (dialog.editingId < 0) dialog.kind = modelData.key
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            visible: dialog.editingId > 0
                            text: qsTr("正在编辑：") + nameInput.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.controlH
                            radius: Theme.radiusBtn
                            color: Theme.bgSurface
                            border.width: 1
                            border.color: nameInput.activeFocus ? Theme.accent : Theme.borderStrong

                            TextInput {
                                id: nameInput
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spaceLg
                                anchors.rightMargin: Theme.spaceSm
                                verticalAlignment: TextInput.AlignVCenter
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                color: Theme.textPrimary
                                selectionColor: Theme.accentSoftStrong
                                onAccepted: dialog.submit()
                                onTextChanged: dialog.errorText = ""

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    visible: nameInput.text.length === 0
                                    text: qsTr("分类名称，如 咖啡、宠物、通勤")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    color: Theme.textTertiary
                                }
                            }
                        }

                        FlatButton {
                            text: dialog.editingId > 0 ? qsTr("保存修改") : qsTr("新增")
                            kind: "primary"
                            onClicked: dialog.submit()
                        }

                        FlatButton {
                            visible: dialog.editingId > 0
                            text: qsTr("取消")
                            kind: "ghost"
                            onClicked: dialog.resetForm()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs

                        Text {
                            text: qsTr("颜色")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        Repeater {
                            model: dialog.colorChoices

                            delegate: Rectangle {
                                required property string modelData
                                width: 20
                                height: 20
                                radius: 10
                                color: modelData
                                border.width: dialog.selectedColor === modelData ? 2 : 0
                                border.color: Theme.textPrimary

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dialog.selectedColor = modelData
                                }
                            }
                        }

                        Text {
                            text: dialog.errorText
                            visible: dialog.errorText.length > 0
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.dangerFg
                        }
                    }
                }
            }

            // 列表
            ListView {
                id: categoryList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: Ledger.managedCategories
                spacing: 1

                delegate: Rectangle {
                    required property var modelData
                    width: categoryList.width
                    height: 34
                    radius: Theme.radiusBtn
                    color: rowMouse.containsMouse ? Theme.bgHover : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spaceSm
                        anchors.rightMargin: Theme.spaceSm
                        spacing: Theme.spaceSm

                        Rectangle {
                            width: 18
                            height: 18
                            radius: 9
                            color: modelData.color && modelData.color.length > 0 ? modelData.color : Theme.textTertiary

                            Text {
                                anchors.centerIn: parent
                                text: modelData.icon && modelData.icon.length > 0 ? modelData.icon
                                                                                  : modelData.name.left(1)
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: Theme.textOnAccent
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.name + (modelData.archived ? "  " + qsTr("（已归档）") : "")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: modelData.archived ? Theme.textTertiary : Theme.textPrimary
                        }

                        Text {
                            text: modelData.kindText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        Text {
                            visible: modelData.builtin
                            text: qsTr("内置")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        FlatButton {
                            text: qsTr("编辑")
                            kind: "ghost"
                            onClicked: dialog.beginEdit(modelData)
                        }

                        FlatButton {
                            text: modelData.archived ? qsTr("恢复") : qsTr("归档")
                            kind: "ghost"
                            onClicked: {
                                const result = Ledger.archiveCategory(modelData.id, !modelData.archived)
                                dialog.notifyRequested(result.message)
                            }
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: categoryList.count === 0
                    text: qsTr("还没有分类")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textTertiary
                }
            }
        }
    }

    Shortcut { sequence: "Escape"; enabled: dialog.visible; onActivated: dialog.close() }
}