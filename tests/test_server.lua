target("test_server")
    mks_apply_test_settings(path.join(os.scriptdir(), "test_server.cpp"))
    add_files(
        path.join(os.scriptdir(), "test_server.cpp"),
        path.join(os.projectdir(), "src/app/server.cpp"),
        path.join(os.projectdir(), "src/app/server_session.cpp"),
        path.join(os.projectdir(), "src/app/server_screens.cpp"),
        path.join(os.projectdir(), "src/app/server_input.cpp"),
        path.join(os.projectdir(), "src/config/app_config.cpp"),
        path.join(os.projectdir(), "src/rpc/message.cpp"),
        path.join(os.projectdir(), "src/rpc/transport.cpp"),
        path.join(os.projectdir(), "src/core/topology.cpp")
    )
target_end()
