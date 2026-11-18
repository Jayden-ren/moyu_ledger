import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 命令面板（Ctrl+K）：单输入框 + 分组命令列表，↑↓ 选择 / Enter 执行 / Esc 关闭
Item {
    id: palette
    anchors.fill: parent
    visible: false
    z: 950

    signal notifyRequested(string message)
    signal navigateRequested(int index)

    property string query: ""
    property int selectedIndex: 0
    property var results: []

    // 命令表：id / 分组 / 标题 / 说明 / 键位 / 关键词（用于匹配）
    readonly property var commands: [
        { id: "new", group: qsTr("快捷录入"), title: qsTr("记一笔"), hint: qsTr("打开记账面板"), keys: "Ctrl N", keywords: "记一笔 新增 add new record" },
        { id: "palette_input", group: qsTr("快捷录入"), title: qsTr("自然语言速记"), hint: qsTr("在面板里输入「午餐 23 支付宝」"), keys: "", keywords: "速记 自然语言 quick input" },

        { id: "nav_workbench", group: qsTr("跳转"), title: qsTr("工作台"), hint: qsTr("流水与明细"), keys: "Ctrl 1", keywords: "工作台 流水 workbench" },
        { id: "nav_overview", group: qsTr("跳转"), title: qsTr("概览"), hint: qsTr("KPI · 预算环 · 趋势"), keys: "Ctrl 2", keywords: "概览 图表 overview" },
        { id: "nav_ledger", group: qsTr("跳转"), title: qsTr("流水"), hint: qsTr("完整流水列表"), keys: "Ctrl 3", keywords: "流水 ledger" },
        { id: "nav_report", group: qsTr("跳转"), title: qsTr("报表"), hint: qsTr("分类排行 · 收支对比"), keys: "Ctrl 4", keywords: "报表 report" },
        { id: "nav_budget", group: qsTr("跳转"), title: qsTr("预算"), hint: qsTr("总预算与分类预算"), keys: "Ctrl 5", keywords: "预算 budget" },
        { id: "nav_accounts", group: qsTr("跳转"), title: qsTr("账户"), hint: qsTr("账户管理与净资产"), keys: "Ctrl 6", keywords: "账户 account" },
        { id: "nav_settings", group: qsTr("跳转"), title: qsTr("设置"), hint: qsTr("主题与数据路径"), keys: "Ctrl 7", keywords: "设置 settings" },

        { id: "filter_food", group: qsTr("操作"), title: qsTr("筛选 · 本月餐饮"), hint: qsTr("按关键字筛选流水"), keys: "", keywords: "筛选 餐饮 filter" },
        { id: "clear_filter", group: qsTr("操作"), title: qsTr("清除筛选"), hint: qsTr("恢复全部流水"), keys: "", keywords: "清除 筛选 clear" },
        { id: "manage_categories", group: qsTr("操作"), title: qsTr("分类管理"), hint: qsTr("新增 / 改名 / 归档分类"), keys: "", keywords: "分类 category" },
        { id: "import_csv", group: qsTr("操作"), title: qsTr("导入账单 CSV"), hint: qsTr("支付宝 / 微信账单"), keys: "", keywords: "导入 账单 import csv" },
        { id: "export_csv", group: qsTr("操作"), title: qsTr("导出本月 CSV"), hint: qsTr("写入「文档」目录"), keys: "Ctrl E", keywords: "导出 csv export" },
        { id: "toggle_dark", group: qsTr("操作"), title: qsTr("切换暗色主题"), hint: qsTr("Ctrl+Shift+L"), keys: "", keywords: "暗色 主题 dark theme" },
        { id: "undo", group: qsTr("操作"), title: qsTr("撤销"), hint: qsTr("回退上一次记账改动"), keys: "Ctrl Z", keywords: "撤销 undo" },
        { id: "redo", group: qsTr("操作"), title: qsTr("重做"), hint: qsTr("恢复被撤销的改动"), keys: "Ctrl Shift Z", keywords: "重做 redo" }
    ]

    function open() {
        query = ""
        selectedIndex = 0
        refresh()
        visible = true
        searchField.forceActiveFocus()
    }

    // 打开并预填查询（便于外部调用/测试）
    function openWith(text) {
        open()
        searchField.text = text
    }

    function close() {
        visible = false
    }

    // 简单打分：标题包含 > 关键词包含 > 标题子序列
    function score(item, q) {
        if (q.length === 0)
            return 1
        const lower = q.toLowerCase()
        const title = item.title.toLowerCase()
        const keys = item.keywords.toLowerCase()
        if (title.indexOf(lower) >= 0)
            return 100 - title.indexOf(lower)
        if (keys.indexOf(lower) >= 0)
            return 60
        let pos = 0
        for (let i = 0; i < title.length && pos < lower.length; ++i) {
            if (title[i] === lower[pos])
                ++pos
        }
        return pos === lower.length ? 20 : -1
    }

    function refresh() {
        const scored = []
        for (let i = 0; i < commands.length; ++i) {
            const s = score(commands[i], query.trim())
            if (s >= 0)
                scored.push({ item: commands[i], value: s })
        }
        scored.sort((a, b) => b.value - a.value)
        const list = []
        for (let i = 0; i < scored.length; ++i)
            list.push(scored[i].item)
        results = list
        if (selectedIndex >= results.length)
            selectedIndex = Math.max(0, results.length - 1)
    }

    function execute(id) {
        switch (id) {
        case "new":
        case "palette_input":
            close()
            recordDialogHook()
            break
        case "nav_workbench": navigateRequested(0); close(); break
        case "nav_overview": navigateRequested(1); close(); break
        case "nav_ledger": navigateRequested(2); close(); break
        case "nav_report": navigateRequested(3); close(); break
        case "nav_budget": navigateRequested(4); close(); break
        case "nav_accounts": navigateRequested(5); close(); break
        case "nav_settings": navigateRequested(6); close(); break
        case "filter_food":
            Ledger.setKeyword("餐饮")
            navigateRequested(0)
            notifyRequested(qsTr("已筛选：关键字「餐饮」"))
            close()
            break
        case "clear_filter":
            Ledger.clearFilter()
            notifyRequested(qsTr("已清除筛选"))
            close()
            break
        case "manage_categories":
            close()
            categoryDialogHook()
            break
        case "import_csv":
            close()
            importDialogHook()
            break
        case "export_csv":
            notifyRequested(Ledger.exportMonthCsv("").message)
            close()
            break
        case "toggle_dark":
            AppContext.toggleDarkTheme()
            notifyRequested(AppContext.darkTheme ? qsTr("已切换到暗色主题") : qsTr("已切换到浅色主题"))
            close()
            break
        case "undo":
            notifyRequested(Ledger.undo().message)
            close()
            break
        case "redo":
            notifyRequested(Ledger.redo().message)
            close()
            break
        }
    }

    // 由 Main.qml 注入「打开记一笔」动作
    signal recordDialogHook()
    signal importDialogHook()
    signal categoryDialogHook()

    // 遮罩
    Rectangle {
        anchors.fill: parent
        color: Theme.scrimSoft
        MouseArea { anchors.fill: parent; onClicked: palette.close() }
    }

    Rectangle {
        width: 640
        anchors.horizontalCenter: parent.horizontalCenter
        y: Theme.titleBarH + Theme.paletteTop - 40
        // 固定高度：不能用 list.height 反推，否则与 ColumnLayout 形成循环依赖被压扁
        height: 520
        radius: Theme.radiusPalette
        color: Theme.bgSurface
        border.width: 1
        border.color: Theme.borderBase

        MouseArea { anchors.fill: parent }  // 吞掉点击

        ColumnLayout {
            id: layout
            anchors.fill: parent
            anchors.margins: Theme.spaceSm
            spacing: 0

            // 输入行
            Item {
                id: header
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.paletteInputH

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceMd
                    anchors.rightMargin: Theme.spaceMd
                    spacing: Theme.spaceSm

                    Text {
                        text: "⌕"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        color: Theme.textTertiary
                    }

                    TextInput {
                        id: searchField
                        Layout.fillWidth: true
                        verticalAlignment: TextInput.AlignVCenter
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody2
                        color: Theme.textPrimary
                        selectionColor: Theme.accentSoftStrong
                        onTextChanged: {
                            palette.query = text
                            palette.selectedIndex = 0
                            palette.refresh()
                        }
                        onAccepted: {
                            if (palette.results.length > 0)
                                palette.execute(palette.results[palette.selectedIndex].id)
                        }
                        Keys.onDownPressed: palette.selectedIndex = Math.min(palette.selectedIndex + 1, palette.results.length - 1)
                        Keys.onUpPressed: palette.selectedIndex = Math.max(palette.selectedIndex - 1, 0)
                        Keys.onEscapePressed: palette.close()

                        Text {
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            visible: searchField.text.length === 0
                            text: qsTr("输入命令，例如「报表」「导出」「记一笔」")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody2
                            color: Theme.textTertiary
                        }
                    }

                    KeyCap { text: "Esc" }
                }

                Rectangle {
                    height: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    color: Theme.borderHairline
                }
            }

            // 结果列表
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 360
                clip: true
                model: palette.results
                spacing: 0
                currentIndex: palette.selectedIndex

                delegate: Rectangle {
                    required property int index
                    required property var modelData

                    width: list.width
                    height: Theme.paletteRowH
                    radius: Theme.radiusBtn
                    color: index === palette.selectedIndex ? Theme.bgSelection : "transparent"

                    Rectangle {
                        visible: index === palette.selectedIndex
                        width: 2
                        height: parent.height - 8
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 1
                        color: Theme.accent
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spaceMd
                        anchors.rightMargin: Theme.spaceMd
                        spacing: Theme.spaceMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                text: modelData.title
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontBody
                                color: index === palette.selectedIndex ? Theme.accent : Theme.textPrimary
                            }
                            Text {
                                text: modelData.group + " · " + modelData.hint
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: Theme.textTertiary
                            }
                        }

                        KeyCap {
                            visible: modelData.keys.length > 0
                            text: modelData.keys
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            palette.selectedIndex = index
                            palette.execute(modelData.id)
                        }
                    }
                }
            }

            // 底部键位
            Item {
                id: footer
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.paletteFooterH

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceMd
                    anchors.rightMargin: Theme.spaceMd
                    spacing: Theme.spaceMd

                    Row {
                        spacing: Theme.spaceXs
                        KeyCap { text: "↑↓" }
                        Text { text: qsTr("选择"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Row {
                        spacing: Theme.spaceXs
                        KeyCap { text: "Enter" }
                        Text { text: qsTr("执行"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Row {
                        spacing: Theme.spaceXs
                        KeyCap { text: "Esc" }
                        Text { text: qsTr("关闭"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary; anchors.verticalCenter: parent.verticalCenter }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: palette.results.length + qsTr(" 条命令")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }
            }
        }
    }
}