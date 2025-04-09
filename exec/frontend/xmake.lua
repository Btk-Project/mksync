add_rules("mode.debug", "mode.release")

target("main")
    add_rules("qt.widgetapp")
    add_headerfiles("src/*.h")
    add_headerfiles("src/*.hpp")
    add_includedirs("src/")
    add_files("src/*.hpp")
    add_files("src/*.cpp")
    set_targetdir("$(bindir)")
    add_rules("target.clean", "target.autoname")
    add_defines("NEKO_PROTO_LOG_CONTEXT")
    add_packages("spdlog", "ilias", "neko-proto-tools", "cxxopts")
    add_deps("proto", "base", "core")
    add_includedirs("../../src/proto/include")
    if is_plat("windows") then 
        add_links("dwmapi")
    end

    add_files("src/mainwindow.ui")
    -- add files with Q_OBJECT meta (only for qt.moc)
    add_files("resource/*.qrc")
    add_files("src/mainwindow.h")
