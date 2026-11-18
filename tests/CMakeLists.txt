# 数据/业务层自检：直接跑应用内置的 --self-test（46 项断言，独立临时库）
add_test(NAME keep_selftest COMMAND $<TARGET_FILE:MoyuLedger> --self-test)

# 让 ctest 能加载 Qt 与 MinGW 运行库（从 Qt6::Core 的位置推出来，不写死路径）
get_target_property(_qt_core_location Qt6::Core LOCATION)
get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
set_tests_properties(keep_selftest PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin_dir}"
    TIMEOUT 120
)