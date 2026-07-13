target("test_backend_registry")
    local test_file = path.join(os.scriptdir(), "test_backend_registry.cpp")
    mks_apply_test_settings(test_file)
    add_files(
        test_file,
        path.join(os.projectdir(), "src/platform/backend.cpp")
    )
target_end()
