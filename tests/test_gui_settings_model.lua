target("test_gui_settings_model")
    local testFile = path.join(os.scriptdir(), "test_gui_settings_model.cpp")
    mks_apply_test_settings(testFile)
    add_includedirs(path.join(os.projectdir(), "ui"))
    add_files(
        testFile,
        path.join(os.projectdir(), "ui/model/settings_model.cpp"),
        path.join(os.projectdir(), "src/config/app_config.cpp")
    )
    -- gmock_main depends on symbols from gmock; keep a final archive occurrence
    -- because the package's default static-library order places it too early.
    add_ldflags("-lgmock", {force = true})
target_end()
