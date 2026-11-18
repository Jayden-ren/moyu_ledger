import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 记一笔（优化版）：支出 / 收入 / 转账 三段
// - 支出/收入：大类 → 小类（可只记大类），时间精确到分钟
// - 转账：本账本账户互转（不计收支）或转给他人（记作支出 / 借出）
Item {
    id: dialog
    anchors.fill: parent
    visible: false
    z: 900

    signal saved(string message)
    signal accountCreationRequested()

    property int editingId: -1
    property string kind: "expense"
    property int bigCategoryId: -1
    property int subCategoryId: -1
    property int selectedAccountId: -1
    property int toAccountId: -1
    property string transferNature: "transfer"
    property string dateText: ""
    property string timeText: "12:00"
    property string errorText: ""

    readonly property bool isTransfer: kind === "transfer"
    readonly property bool isLend: isTransfer && transferNature === "lend"
    readonly property bool isTransferExpense: isTransfer && transferNature === "expense"
    readonly property bool needsCategory: !isTransfer || isTransferExpense
    readonly property var tree: kind === "income" ? Ledger.incomeCategoryTree : Ledger.expenseCategoryTree
    readonly property var subs: {
        const list = tree || []
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === bigCategoryId)
                return list[i].children || []
        }
        return []
    }

    function firstBigId() {
        const list = tree || []
        return list.length > 0 ? list[0].id : -1
    }

    function bigCategoryName() {
        const list = dialog.tree || []
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === dialog.bigCategoryId)
                return list[i].name
        }
        return ""
    }

    function subCategoryName() {
        const list = dialog.subs || []
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === dialog.subCategoryId)
                return list[i].name
        }
        return ""
    }

    function hintText() {
        if (dialog.isTransfer) {
            if (dialog.transferNature === "transfer")
                return qsTr("本账本账户互转：不计收支与预算，只改两个账户余额。")
            if (dialog.transferNature === "lend")
                return qsTr("借出：转入「应收」账户，不计支出；对方还款时反向转回即可冲销。")
            return qsTr("转给他人 · 记作支出：计入本月支出与预算，分类建议「人情往来 › 借出」。")
        }
        const sub = dialog.subCategoryName()
        if (sub.length > 0)
            return qsTr("已选小类：") + dialog.bigCategoryName() + " › " + sub
        return qsTr("未选小类，只记大类「") + dialog.bigCategoryName() + "」"
    }

    function resetForm() {
        editingId = -1
        kind = "expense"
        bigCategoryId = firstBigId()
        subCategoryId = -1
        selectedAccountId = Ledger.defaultAccountId()
        toAccountId = -1
        transferNature = "transfer"
        amountText = ""
        const now = new Date()
        dateText = Qt.formatDate(now, "yyyy-MM-dd")
        timeText = Qt.formatTime(now, "HH:mm")
        noteInput.text = ""
        tagsInput.text = ""
        errorText = ""
    }

    property string amountText: ""

    function open() {
        if (Ledger.accountModel.count === 0) {
            accountCreationRequested()
            return
        }
        resetForm()
        visible = true
        amountInput.forceActiveFocus()
    }

    function openForEdit(detail) {
        if (!detail || detail.transactionId === undefined)
            return
        resetForm()
        editingId = detail.transactionId
        kind = detail.kind === "income" ? "income"
               : (detail.kind === "transfer" ? "transfer" : "expense")
        selectedAccountId = detail.accountId || -1
        amountText = (Math.abs(detail.amountCents || 0) / 100).toFixed(2).replace(/\.00$/, "")
        amountInput.text = amountText
        dateText = (detail.dateText || "").substring(0, 10)
        timeText = detail.occurredText || "12:00"
        noteInput.text = detail.note || ""
        tagsInput.text = detail.tagsText || ""
        if (kind === "transfer") {
            toAccountId = detail.toAccountId || -1
            transferNature = toAccountId > 0 ? "lend" : "transfer"
        }
        bigCategoryId = firstBigId()
        subCategoryId = -1
        const categoryId = detail.categoryId || -1
        if (categoryId > 0) {
            const list = tree || []
            for (let i = 0; i < list.length; ++i) {
                if (list[i].id === categoryId) {
                    bigCategoryId = categoryId
                    break
                }
                const children = list[i].children || []
                for (let j = 0; j < children.length; ++j) {
                    if (children[j].id === categoryId) {
                        bigCategoryId = list[i].id
                        subCategoryId = categoryId
                    }
                }
            }
        }
        visible = true
        amountInput.forceActiveFocus()
    }

    function close() { visible = false }

    function pickDate(offsetDays) {
        const day = new Date()
        day.setDate(day.getDate() - offsetDays)
        dateText = Qt.formatDate(day, "yyyy-MM-dd")
        if (offsetDays === 0)
            timeText = Qt.formatTime(day, "HH:mm")
    }

    function submit() {
        // 日期/时间由弹框写回 state，不能再引用已被替换掉的 TextInput
        const dateValue = dialog.dateText.trim()
        const timeValue = dialog.timeText.trim()
        if (!/^\d{4}-\d{2}-\d{2}$/.test(dateValue)) {
            errorText = qsTr("日期格式应为 yyyy-MM-dd")
            return
        }
        if (!/^\d{1,2}:\d{2}$/.test(timeValue)) {
            errorText = qsTr("时间格式应为 HH:mm")
            return
        }
        if (dialog.needsCategory && dialog.bigCategoryId <= 0) {
            errorText = qsTr("请选择支出大类")
            return
        }

        const entry = {
            "kind": kind,
            "amountText": amountInput.text,
            "accountId": selectedAccountId,
            "categoryId": needsCategory ? (subCategoryId > 0 ? subCategoryId : bigCategoryId) : -1,
            "toAccountId": (transferNature === "transfer" || transferNature === "lend") ? toAccountId : -1,
            "dateText": dateValue,
            "timeText": timeValue,
            "note": noteInput.text,
            "tags": tagsInput.text,
            "transferNature": isTransfer ? transferNature : "transfer"
        }
        const result = editingId > 0 ? Ledger.updateEntry(editingId, entry)
                                    : Ledger.saveEntry(entry)
        if (result.ok) {
            close()
            dialog.saved(result.message)
        } else {
            errorText = result.message
        }
    }

    readonly property var kindOptions: [
        { key: "expense", label: qsTr("支出") },
        { key: "income", label: qsTr("收入") },
        { key: "transfer", label: qsTr("转账") }
    ]
    // 遮罩
    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent; onClicked: dialog.close() }
    }

    Rectangle {
        id: panel
        width: 920
        height: 612
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.bgSurface
        border.width: 1
        border.color: Theme.borderBase

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spaceXl
            spacing: Theme.spaceS

            // 头部
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd

                Text {
                    text: dialog.editingId > 0 ? qsTr("编辑一笔") : qsTr("记一笔")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH1
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                Rectangle {
                    height: 30
                    width: kindRow.implicitWidth + Theme.spaceS * 2
                    radius: 6
                    color: Theme.bgSubtle

                    Row {
                        id: kindRow
                        anchors.centerIn: parent
                        spacing: 2

                        Repeater {
                            model: dialog.kindOptions

                            delegate: Rectangle {
                                required property var modelData
                                width: segLabel.implicitWidth + Theme.spaceXl
                                height: 26
                                radius: 4
                                color: dialog.kind === modelData.key ? Theme.bgSurface : "transparent"

                                Text {
                                    id: segLabel
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    font.weight: dialog.kind === modelData.key ? Theme.weightSemibold
                                                                               : Theme.weightRegular
                                    color: dialog.kind === modelData.key ? Theme.accent : Theme.textSecondary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        dialog.kind = modelData.key
                                        dialog.transferNature = "transfer"
                                        dialog.subCategoryId = -1
                                        dialog.errorText = ""
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: dialog.isTransfer ? qsTr("账户之间移动，不算消费")
                                            : qsTr("Ctrl+Enter 保存 · Esc 取消")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Rectangle {
                    width: 28
                    height: 28
                    radius: 6
                    color: closeMouse.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textSecondary
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dialog.close()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderHairline }

            // 主体
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spaceXl

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.spaceXs

                    // 大类
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs
                        visible: dialog.needsCategory

                        Text {
                            text: dialog.kind === "income" ? qsTr("收入大类") : qsTr("支出大类")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 5
                            columnSpacing: Theme.spaceS
                            rowSpacing: Theme.spaceS

                            Repeater {
                                model: dialog.tree

                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    height: 50
                                    radius: 8
                                    color: dialog.bigCategoryId === modelData.id ? Theme.accentSoft : Theme.bgSurface
                                    border.width: 1
                                    border.color: dialog.bigCategoryId === modelData.id ? Theme.accent : Theme.borderStrong

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 1

                                        Text {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: modelData.icon && modelData.icon.length > 0 ? modelData.icon
                                                                                               : modelData.name.left(1)
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSmall2
                                            font.weight: Theme.weightMedium
                                            color: modelData.color && modelData.color.length > 0 ? modelData.color
                                                                                                 : Theme.textSecondary
                                        }

                                        Text {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: modelData.name
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontMicro3
                                            color: dialog.bigCategoryId === modelData.id ? Theme.accent
                                                                                        : Theme.textSecondary
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            dialog.bigCategoryId = modelData.id
                                            dialog.subCategoryId = -1
                                        }
                                    }
                                }
                            }

                            // 新建大类
                            Rectangle {
                                Layout.fillWidth: true
                                height: 50
                                radius: 8
                                color: Theme.bgSurface
                                border.width: 1
                                border.color: Theme.borderBase

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("＋ 新建大类")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dialog.openNewCategory(-1)
                                }
                            }
                        }
                    }

                    // 小类
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs
                        visible: dialog.needsCategory

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("小类（可不选，只记大类）")
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: Theme.textTertiary
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: {
                                    const sub = dialog.subCategoryName()
                                    return sub.length > 0
                                           ? dialog.bigCategoryName() + " › " + sub
                                           : qsTr("只记大类：") + dialog.bigCategoryName()
                                }
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: Theme.accent
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                width: onlyBigLabel.implicitWidth + Theme.spaceLg
                                height: 26
                                radius: 13
                                color: dialog.subCategoryId < 0 ? Theme.bgSubtle : Theme.bgSurface
                                border.width: 1
                                border.color: dialog.subCategoryId < 0 ? Theme.borderStrong : Theme.borderBase

                                Text {
                                    id: onlyBigLabel
                                    anchors.centerIn: parent
                                    text: qsTr("只记大类")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: dialog.subCategoryId < 0 ? Theme.textPrimary : Theme.textTertiary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dialog.subCategoryId = -1
                                }
                            }

                            Repeater {
                                model: dialog.subs

                                delegate: Rectangle {
                                    required property var modelData
                                    width: subLabel.implicitWidth + Theme.spaceLg
                                    height: 26
                                    radius: 13
                                    color: dialog.subCategoryId === modelData.id ? Theme.accent : Theme.bgSurface
                                    border.width: 1
                                    border.color: dialog.subCategoryId === modelData.id ? Theme.accent : Theme.borderBase

                                    Text {
                                        id: subLabel
                                        anchors.centerIn: parent
                                        text: modelData.name
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontMicro3
                                        color: dialog.subCategoryId === modelData.id ? Theme.textOnAccent
                                                                                     : Theme.textPrimary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: dialog.subCategoryId = modelData.id
                                    }
                                }
                            }

                            // 新建小类
                            Rectangle {
                                width: newSubLabel.implicitWidth + Theme.spaceLg
                                height: 26
                                radius: 13
                                color: Theme.bgSurface
                                border.width: 1
                                border.color: Theme.borderBase

                                Text {
                                    id: newSubLabel
                                    anchors.centerIn: parent
                                    text: qsTr("＋ 新建小类")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dialog.openNewCategory(dialog.bigCategoryId)
                                }
                            }
                        }
                    }
                    // 转账：性质 + 记作
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs
                        visible: dialog.isTransfer

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [{ key: "transfer", label: qsTr("本账本互转") },
                                        { key: "expense", label: qsTr("转给他人") }]

                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    height: 36
                                    radius: 8
                                    color: (dialog.transferNature === "transfer" && modelData.key === "transfer")
                                           || (dialog.transferNature !== "transfer" && modelData.key === "expense")
                                           ? Theme.accentSoft : Theme.bgSurface
                                    border.width: 1
                                    border.color: (dialog.transferNature === "transfer" && modelData.key === "transfer")
                                                  || (dialog.transferNature !== "transfer" && modelData.key === "expense")
                                                  ? Theme.accent : Theme.borderStrong

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSmall2
                                        color: Theme.textPrimary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            dialog.transferNature = modelData.key === "transfer" ? "transfer" : "expense"
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: dialog.transferNature !== "transfer"

                            Repeater {
                                model: [{ key: "expense", label: qsTr("记作 · 支出（默认）") },
                                        { key: "lend", label: qsTr("记作 · 借出") }]

                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    height: 32
                                    radius: 8
                                    color: dialog.transferNature === modelData.key ? Theme.accentSoft : Theme.bgSurface
                                    border.width: 1
                                    border.color: dialog.transferNature === modelData.key ? Theme.accent : Theme.borderStrong

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontMicro2
                                        color: dialog.transferNature === modelData.key ? Theme.accent
                                                                                        : Theme.textSecondary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: dialog.transferNature = modelData.key
                                    }
                                }
                            }
                        }
                    }

                    // 账户（转账时为「从账户」）
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs

                        Text {
                            text: dialog.isTransfer ? qsTr("从账户")
                                                    : (dialog.kind === "income" ? qsTr("收入账户") : qsTr("付款账户"))
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: Ledger.accountModel

                                delegate: Rectangle {
                                    required property string name
                                    required property int accountId
                                    width: fromLabel.implicitWidth + Theme.spaceLg
                                    height: 26
                                    radius: Theme.radiusBtn
                                    color: dialog.selectedAccountId === accountId ? Theme.accentSoft : Theme.bgSurface
                                    border.width: 1
                                    border.color: dialog.selectedAccountId === accountId ? Theme.accent
                                                                                          : Theme.borderStrong

                                    Text {
                                        id: fromLabel
                                        anchors.centerIn: parent
                                        text: name
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontMicro3
                                        color: dialog.selectedAccountId === accountId ? Theme.accent
                                                                                      : Theme.textPrimary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: dialog.selectedAccountId = accountId
                                    }
                                }
                            }
                        }
                    }

                    // 转入账户（互转 / 借出）
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs
                        visible: dialog.isTransfer && dialog.transferNature !== "expense"

                        Text {
                            text: dialog.isLend ? qsTr("转入往来账户（借出记入应收）") : qsTr("转入账户")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: Ledger.accountModel

                                delegate: Rectangle {
                                    required property string name
                                    required property int accountId
                                    required property string accountType

                                    visible: dialog.isLend
                                             ? (accountType === "receivable" || accountType === "payable")
                                             : accountId !== dialog.selectedAccountId
                                    width: toLabel.implicitWidth + Theme.spaceLg
                                    height: 26
                                    radius: Theme.radiusBtn
                                    color: dialog.toAccountId === accountId ? Theme.accentSoft : Theme.bgSurface
                                    border.width: 1
                                    border.color: dialog.toAccountId === accountId ? Theme.accent
                                                                                   : Theme.borderStrong

                                    Text {
                                        id: toLabel
                                        anchors.centerIn: parent
                                        text: name
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontMicro3
                                        color: dialog.toAccountId === accountId ? Theme.accent
                                                                                : Theme.textPrimary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: dialog.toAccountId = accountId
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: dialog.isLend
                            wrapMode: Text.WordWrap
                            text: qsTr("没有往来账户时，先去「账户」页新建一个「应收」账户")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }

                    // 时间
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: qsTr("时间")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }

                        Rectangle {
                            id: dateField
                            Layout.preferredWidth: 128
                            Layout.preferredHeight: 30
                            radius: Theme.radiusBtn
                            color: dateFieldMouse.containsMouse ? Theme.bgHover : Theme.bgSurface
                            border.width: 1
                            border.color: datePicker.visible ? Theme.accent : Theme.borderStrong

                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "📅"
                                    font.pixelSize: 11
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: dialog.dateText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro2
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                }
                            }

                            MouseArea {
                                id: dateFieldMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: datePicker.show(dialog.dateText)
                            }
                        }

                        Rectangle {
                            id: timeField
                            Layout.preferredWidth: 84
                            Layout.preferredHeight: 30
                            radius: Theme.radiusBtn
                            color: timeFieldMouse.containsMouse ? Theme.bgHover : Theme.bgSurface
                            border.width: 1
                            border.color: timePicker.visible ? Theme.accent : Theme.borderStrong

                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "🕐"
                                    font.pixelSize: 11
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: dialog.timeText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro2
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                }
                            }

                            MouseArea {
                                id: timeFieldMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: timePicker.show(dialog.timeText)
                            }
                        }

                        Repeater {
                            model: [{ label: qsTr("现在"), offset: 0 },
                                    { label: qsTr("昨天"), offset: 1 },
                                    { label: qsTr("前天"), offset: 2 }]

                            delegate: Rectangle {
                                required property var modelData
                                Layout.preferredWidth: 46
                                Layout.preferredHeight: 30
                                radius: Theme.radiusBtn
                                color: Theme.bgSurface
                                border.width: 1
                                border.color: Theme.borderStrong

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textSecondary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dialog.pickDate(modelData.offset)
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // 备注
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: noteInput.activeFocus ? Theme.accent : Theme.borderStrong

                        TextInput {
                            id: noteInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            anchors.rightMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            onAccepted: dialog.submit()
                        }

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: Text.AlignVCenter
                            visible: noteInput.text.length === 0
                            text: qsTr("备注（可选）")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textTertiary
                        }
                    }

                    // 标签
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: tagsInput.activeFocus ? Theme.accent : Theme.borderBase

                        TextInput {
                            id: tagsInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro2
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                        }

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: Text.AlignVCenter
                            visible: tagsInput.text.length === 0
                            text: qsTr("标签（可选，逗号分隔）")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro2
                            color: Theme.textTertiary
                        }
                    }

                    // 已有标签快捷 chip
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 2
                        spacing: 6
                        visible: Ledger.allTags.length > 0

                        Repeater {
                            model: Ledger.allTags.length > 8 ? 8 : Ledger.allTags.length

                            delegate: Rectangle {
                                required property int index
                                width: quickTag.implicitWidth + Theme.spaceLg
                                height: 22
                                radius: 11
                                color: tagMouse.containsMouse ? Theme.bgHover : Theme.bgSubtle
                                border.width: 1
                                border.color: Theme.borderBase

                                Text {
                                    id: quickTag
                                    anchors.centerIn: parent
                                    text: "# " + Ledger.allTags[index]
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textSecondary
                                }

                                MouseArea {
                                    id: tagMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        const tag = Ledger.allTags[index]
                                        const current = tagsInput.text.trim()
                                        if (current.indexOf(tag) >= 0)
                                            return
                                        tagsInput.text = current.length === 0 ? tag : current + "、" + tag
                                    }
                                }
                            }
                        }
                    }
                }
                // 右列：金额 + 提示
                ColumnLayout {
                    Layout.preferredWidth: 236
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.spaceXs

                    Text {
                        text: dialog.isTransfer ? qsTr("转账金额") : qsTr("金额")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: "¥"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontTitle
                            color: Theme.textTertiary
                        }

                        TextInput {
                            id: amountInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: 34
                            font.weight: Theme.weightSemibold
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: RegularExpressionValidator {
                                regularExpression: /[0-9]{0,9}(\.[0-9]{0,2})?/
                            }
                            onAccepted: dialog.submit()

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                visible: amountInput.text.length === 0
                                text: "0.00"
                                font.family: Theme.fontFamily
                                font.pixelSize: 34
                                font.weight: Theme.weightSemibold
                                color: Theme.textTertiary
                            }
                        }
                    }

                    // 数字键盘
                    GridLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spaceXs
                        columns: 3
                        columnSpacing: 6
                        rowSpacing: 6

                        Repeater {
                            model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "⌫"]

                            delegate: Rectangle {
                                required property string modelData
                                Layout.fillWidth: true
                                height: 36
                                radius: 7
                                color: keyMouse.containsMouse ? Theme.bgHover : Theme.bgSurface
                                border.width: 1
                                border.color: Theme.borderStrong

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.family: Theme.fontFamily
                                    font.pixelSize: modelData === "⌫" ? 14 : 16
                                    font.weight: Theme.weightMedium
                                    color: modelData === "⌫" ? Theme.textSecondary : Theme.textPrimary
                                }

                                MouseArea {
                                    id: keyMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dialog.pressKey(modelData)
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderHairline }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: dialog.hintText()
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: dialog.errorText.length > 0
                        wrapMode: Text.WordWrap
                        text: dialog.errorText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.dangerFg
                    }

                    Item { Layout.fillHeight: true }

                    // 本月该大类支出 / 分类预算
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 62
                        visible: dialog.needsCategory
                        radius: 8
                        color: Theme.bgSubtle
                        border.width: 1
                        border.color: Theme.borderBase

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spaceS
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTr("本月") + dialog.bigCategoryName() + qsTr("（含小类）")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    readonly property var summary: Ledger.categoryBudgetSummary(dialog.bigCategoryId)
                                    text: !summary.hasBudget ? qsTr("未设预算")
                                          : (summary.overLimit ? qsTr("超支") : qsTr("预算内"))
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: !summary.hasBudget ? Theme.textTertiary
                                           : (summary.overLimit ? Theme.dangerFg : Theme.income)
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    readonly property var summary: Ledger.categoryBudgetSummary(dialog.bigCategoryId)
                                    text: summary.spentText || "¥0.00"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Theme.weightSemibold
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                }
                                Text {
                                    readonly property var summary: Ledger.categoryBudgetSummary(dialog.bigCategoryId)
                                    text: summary.hasBudget
                                          ? (" / " + summary.budgetText + " · " + qsTr("剩 ") + summary.remainText)
                                          : ""
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }
                                Item { Layout.fillWidth: true }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 4
                                radius: 2
                                color: Theme.chartTrack
                                visible: {
                                    const summary = Ledger.categoryBudgetSummary(dialog.bigCategoryId)
                                    return summary.hasBudget === true
                                }

                                Rectangle {
                                    readonly property var summary: Ledger.categoryBudgetSummary(dialog.bigCategoryId)
                                    width: parent.width * Math.max(0, Math.min(1, summary.ratio || 0))
                                    height: parent.height
                                    radius: 2
                                    color: summary.overLimit ? Theme.dangerFg : Theme.accent
                                }
                            }
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderHairline }

            // 底部
            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("小类可不选；账户互转不计收支；给他人转账默认记作支出，可切「借出」")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Row {
                    spacing: Theme.spaceMd

                    FlatButton { text: qsTr("取消"); kind: "ghost"; onClicked: dialog.close() }
                    FlatButton {
                        text: dialog.editingId > 0 ? qsTr("保存修改") : qsTr("保存")
                        kind: "primary"
                        onClicked: dialog.submit()
                    }
                }
            }
        }
    }

    TimePicker {
        id: timePicker
        onPicked: function(value) { dialog.timeText = value }
    }

    DatePicker {
        id: datePicker
        onPicked: function(value) { dialog.dateText = value }
    }

    // 新建大类 / 小类
    Item {
        id: newCategoryPopup
        anchors.fill: parent
        visible: false
        z: 940

        property int parentId: -1
        property string errorText: ""

        function open(parent) {
            parentId = parent
            errorText = ""
            newNameInput.text = ""
            visible = true
            newNameInput.forceActiveFocus()
        }

        function close() { visible = false }

        function inputText() { return newNameInput.text }

        Rectangle {
            anchors.fill: parent
            color: Theme.scrimSoft
            MouseArea { anchors.fill: parent; onClicked: newCategoryPopup.close() }
        }

        Rectangle {
            width: 330
            height: 172
            anchors.centerIn: parent
            radius: Theme.radiusPanel
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderBase

            MouseArea { anchors.fill: parent }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spaceXl
                spacing: Theme.spaceS

                Text {
                    text: newCategoryPopup.parentId > 0
                          ? qsTr("新建小类（归入「") + dialog.bigCategoryName() + qsTr("」）")
                          : qsTr("新建大类")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    radius: Theme.radiusBtn
                    color: Theme.bgSurface
                    border.width: 1
                    border.color: newNameInput.activeFocus ? Theme.accent : Theme.borderStrong

                    TextInput {
                        id: newNameInput
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spaceSm
                        verticalAlignment: TextInput.AlignVCenter
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textPrimary
                        selectionColor: Theme.accentSoftStrong
                        onAccepted: dialog.createCategory()
                        onTextChanged: newCategoryPopup.errorText = ""

                        Text {
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            visible: newNameInput.text.length === 0
                            text: qsTr("名称，如「宠物」或「猫粮」")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textTertiary
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: newCategoryPopup.errorText.length > 0
                    text: newCategoryPopup.errorText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.dangerFg
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Row {
                        spacing: Theme.spaceMd
                        FlatButton { text: qsTr("取消"); kind: "ghost"; onClicked: newCategoryPopup.close() }
                        FlatButton { text: qsTr("保存"); kind: "primary"; onClicked: dialog.createCategory() }
                    }
                }
            }
        }
    }

    function pressKey(key) {
        if (key === "⌫") {
            amountInput.text = amountInput.text.slice(0, -1)
            return
        }
        if (key === "." && amountInput.text.indexOf(".") >= 0)
            return
        if (amountInput.text.length >= 12)
            return
        amountInput.text = amountInput.text + key
    }

    function openNewCategory(parent) {
        newCategoryPopup.open(parent)
    }

    function createCategory() {
        const name = newCategoryPopup.inputText().trim()
        if (name.length === 0) {
            newCategoryPopup.errorText = qsTr("请输入名称")
            return
        }
        const result = Ledger.addCategory(dialog.kind === "income" ? "income" : "expense",
                                          name, "", newCategoryPopup.parentId)
        if (!result.ok) {
            newCategoryPopup.errorText = result.message
            return
        }
        if (newCategoryPopup.parentId > 0) {
            dialog.bigCategoryId = newCategoryPopup.parentId
            dialog.subCategoryId = result.id
        } else {
            dialog.bigCategoryId = result.id
            dialog.subCategoryId = -1
        }
        newCategoryPopup.close()
        dialog.saved(result.message)
    }

    Shortcut { sequence: "Ctrl+Return"; enabled: dialog.visible; onActivated: dialog.submit() }
    Shortcut { sequence: "Escape"; enabled: dialog.visible; onActivated: dialog.close() }
}
