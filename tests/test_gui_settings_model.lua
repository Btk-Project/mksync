target("test_gui_settings_model")
    local testFile = path.join(os.scriptdir(), "test_gui_settings_model.cpp")
    mks_apply_test_settings(testFile)
    add_includedirs(path.join(os.projectdir(), "ui"))
    add_files(
        testFile,
        path.join(os.scriptdir(), "support/gtest_entry.cpp"),
        path.join(os.projectdir(), "ui/model/settings_model.cpp"),
        path.join(os.projectdir(), "src/config/app_config.cpp"),
        path.join(os.projectdir(), "src/config/arg_config.cpp")
    )
target_end()
