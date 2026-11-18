cmake_minimum_required(VERSION 3.19)
project(moyu_ledger VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Sql)

qt_standard_project_setup()

# 纯 C++ 核心库：数据层（不依赖 QML，可被单元测试直接链接）
qt_add_library(keep_core STATIC
    src/core/data/Database.h
    src/core/data/Database.cpp
    src/core/data/Migrator.h
    src/core/data/Migrator.cpp
    src/core/domain/Money.h
    src/core/domain/Money.cpp
    src/core/domain/Domain.h
    src/core/data/Mapping.h
    src/core/data/AccountRepository.h
    src/core/data/AccountRepository.cpp
    src/core/data/AccountDailyRecordRepository.h
    src/core/data/AccountDailyRecordRepository.cpp
    src/core/data/CategoryRepository.h
    src/core/data/CategoryRepository.cpp
    src/core/data/TransactionRepository.h
    src/core/data/TransactionRepository.cpp
    src/core/data/DemoSeeder.h
    src/core/data/DemoSeeder.cpp
    src/core/data/BudgetRepository.h
    src/core/data/BudgetRepository.cpp
    src/core/data/ImportService.h
    src/core/data/ImportService.cpp
    src/core/data/TagRepository.h
    src/core/data/TagRepository.cpp
    src/core/data/SettingsRepository.h
    src/core/data/SettingsRepository.cpp
    src/core/data/RecurringRepository.h
    src/core/data/RecurringRepository.cpp
    src/core/data/LedgerRepository.h
    src/core/data/LedgerRepository.cpp
    src/core/data/BackupService.h
    src/core/data/BackupService.cpp
    src/core/model/TransactionListModel.h
    src/core/model/TransactionListModel.cpp
    src/core/model/AccountListModel.h
    src/core/model/AccountListModel.cpp
    src/core/model/LedgerViewModel.h
    src/core/model/LedgerViewModel.cpp
)

target_link_libraries(keep_core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql)
target_include_directories(keep_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)


# QML 模块与 C++ 上下文（模块根目录 = src/ui，资源路径 qrc:/MoyuLedger/...）
add_subdirectory(src/ui)

enable_testing()
add_subdirectory(tests)

target_compile_definitions(MoyuLedger PRIVATE
    MOYU_LEDGER_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(MoyuLedger
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Qml
        Qt::Quick
        keep_core
)

install(TARGETS MoyuLedger
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)

# 部署：安装目录采用「平铺」布局（exe 与 Qt 运行库同层）。
# 注意：windeployqt 的 --libdir/--plugindir/--qml-deploy-dir 与 --dir 一起用时
# 路径会按当前工作目录解析（实测把文件写到了项目根），因此这里只用 --dir。
get_target_property(_keep_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_keep_qt_bin_dir "${_keep_qmake}" DIRECTORY)
set(_keep_windeployqt "${_keep_qt_bin_dir}/windeployqt.exe")

install(CODE "
  message(STATUS \"Running windeployqt for MoyuLedger ...\")
  execute_process(
    COMMAND \"${_keep_windeployqt}\"
            \"\${CMAKE_INSTALL_PREFIX}/MoyuLedger.exe\"
            --dir \"\${CMAKE_INSTALL_PREFIX}\"
            --qmldir \"${CMAKE_SOURCE_DIR}/src/ui\"
            --release
            --force
    RESULT_VARIABLE _keep_deploy_result
    OUTPUT_QUIET
  )
  if (NOT _keep_deploy_result EQUAL 0)
    message(WARNING \"windeployqt exit code \${_keep_deploy_result}\")
  endif()
")
