target("test_config")
    local test_file = path.join(os.scriptdir(), "test_config.cpp")
    mks_apply_test_settings(test_file)
    add_files(
        test_file,
        path.join(os.projectdir(), "src/config/app_config.cpp")
    )
target_end()
