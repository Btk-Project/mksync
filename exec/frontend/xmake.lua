add_rules("mode.debug", "mode.release")

target("main")
    add_rules("qt.widgetapp")
    add_headerfiles("src/*.h")
    add_files("src/*.cpp")
    set_targetdir("$(bindir)")
    add_rules("target.clean", "target.autoname")

    add_files("src/mainwindow.ui")
    -- add files with Q_OBJECT meta (only for qt.moc)
    add_files("resource/*.qrc")
    add_files("src/mainwindow.h")
