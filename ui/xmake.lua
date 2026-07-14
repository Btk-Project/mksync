-- This file owns every Qt-specific build detail. Keep GUI sources under ui/
-- so the command-line target and its source tree remain framework-free.
target("mksync-gui")
    set_kind("binary")
    set_default(false)
    set_group("applications")
    set_installdir(path.join(os.projectdir(), "build", "install"))

    -- Xmake's Qt Quick rule runs rcc and moc and discovers the system Qt SDK.
    -- Explicit modules make this a Qt 6 Quick Controls application, including
    -- the QML FileDialog module used for import/export.
    add_rules("qt.quickapp")
    add_frameworks("QtCore", "QtGui", "QtQml", "QtQuick", "QtQuickControls2", "QtQuickDialogs2")

    add_packages("ilias", "spdlog", "neko-proto-tools")
    mks_add_backend_packages()
    if not has_config("has_std_format") then
        add_packages("fmt")
    end

    add_includedirs(os.scriptdir(), path.join(os.projectdir(), "src"))
    add_files(
        "main.cpp",
        "model/**.cpp",
        "presentation/**.cpp",
        "presentation/**.hpp",
        "qml.qrc",
        path.join(os.projectdir(), "src/config/app_config.cpp"),
        path.join(os.projectdir(), "src/config/arg_config.cpp"),
        path.join(os.projectdir(), "src/core.cpp"),
        path.join(os.projectdir(), "src/core/**.cpp"),
        path.join(os.projectdir(), "src/app/**.cpp"),
        path.join(os.projectdir(), "src/rpc/**.cpp")
    )
    mks_add_backend_sources()

    -- Use the same generated project configuration and reflection setting as
    -- the CLI because the GUI reuses AppConfig byte-for-byte.
    if stdcxx_version() == 26 and is_tool("cxx", "gcc") then
        add_cxxflags("-freflection", {force = true})
    end

    if is_plat("mingw", "linux") then
        add_links("stdc++exp")
    end

    if is_plat("linux") then
        set_policy("install.strip_packagelibs", true)
        add_rpathdirs("$ORIGIN/../lib", {installonly = true})
    end

    -- Xmake's Qt xpack hook deploys Qt automatically on Windows/macOS. Linux packages use
    -- distro Qt, so install the executable through the normal xpack binary path and retain
    -- the package-local rpath for non-system shared dependencies.
    on_installcmd("linux", function (target, batchcmds, opt)
        import("plugins.pack.batchcmds", {
            alias = "pack_batchcmds",
            rootdir = os.programdir()
        })
        local package = opt.package
        local bindir = package:installdir("bin")
        batchcmds:cp(target:targetfile(), path.join(bindir, target:filename()))
        pack_batchcmds.install_target_shared_libraries(target, batchcmds, {
            package = package
        })
        pack_batchcmds.install_target_files(target, batchcmds, opt)
        pack_batchcmds.update_target_install_rpath(target, batchcmds, opt)
    end)

target_end()
