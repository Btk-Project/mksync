if is_plat("linux") then
target("_package_deb_inputs")
    set_kind("phony")
    set_default(false)
    set_group("packaging/internal")
    add_deps("mksync")
    if has_config("enable_gui") then
        add_deps("mksync-gui")
    end
target_end()
end

task("package_deb")
    set_category("plugin")
    on_run(function()
        import("core.base.option")
        import("core.base.task")
        import("core.project.config")
        local packageDeb = import("packaging.debian.package", {rootdir = os.projectdir()})

        task.run("build", {target = "_package_deb_inputs"})
        assert(config.plat() == "linux", "package_deb is only available for Linux builds")
        packageDeb({
            bundleQt = option.get("bundle-qt"),
            enableGui = config.get("enable_gui"),
            outputdir = option.get("output-dir")
        })
    end)
    set_menu {
        usage = "xmake package_deb [options]",
        description = "Build and create the mksync Debian binary package.",
        options = {
            {'o', "output-dir", "kv", nil, "Set the package output directory. (default: dist)"},
            {nil, "bundle-qt", "k", nil, "Bundle Qt/QML instead of using distribution packages."}
        }
    }

