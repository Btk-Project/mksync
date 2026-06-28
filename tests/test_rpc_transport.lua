target("test_rpc_transport")
    local test_file = path.join(os.scriptdir(), "test_rpc_transport.cpp")
    mks_apply_test_settings(test_file)
    add_files(test_file)
    add_files(
        path.join(os.projectdir(), "src/rpc/message.cpp"),
        path.join(os.projectdir(), "src/rpc/transport.cpp")
    )
target_end()
