target("test_input_pipeline")
    local test_file = path.join(os.scriptdir(), "test_input_pipeline.cpp")
    mks_apply_test_settings(test_file)
    add_defines("MKS_ENABLE_TEST_HOOKS")
    add_files(test_file)
    add_files(
        path.join(os.projectdir(), "src/app/client.cpp"),
        path.join(os.projectdir(), "src/app/server.cpp"),
        path.join(os.projectdir(), "src/config/app_config.cpp"),
        path.join(os.projectdir(), "src/rpc/message.cpp"),
        path.join(os.projectdir(), "src/rpc/transport.cpp"),
        path.join(os.projectdir(), "src/core/topology.cpp")
    )
target_end()
