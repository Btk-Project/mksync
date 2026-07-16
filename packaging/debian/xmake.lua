if is_plat("linux") then
option("deb_outputdir")
    set_default(path.join(os.projectdir(), "dist"))
    set_showmenu(true)
    set_description("Output directory for the repository-owned Debian binary package")
    set_category("packaging")
option_end()

target("package_deb")
    set_kind("phony")
    set_default(false)
    set_group("packaging")
    add_deps("mksync")
    if has_config("enable_gui") then
        add_deps("mksync-gui")
    end

    on_build(function ()
        import("core.project.project")

        local cliTarget = assert(project.target("mksync"), "mksync target is unavailable")
        local iliasPackage = assert(cliTarget:pkg("ilias"), "mksync has no ilias package")
        local iliasDir = assert(iliasPackage:installdir(), "ilias package has no install directory")
        local script = path.join(os.projectdir(), "packaging", "debian", "build.sh")
        local outputdir = get_config("deb_outputdir")
        local arguments = {
            "--cli", cliTarget:targetfile(),
            "--private-library-dir", path.join(iliasDir, "lib"),
            "--output-dir", outputdir
        }

        if has_config("enable_gui") then
            local guiTarget = assert(
                project.target("mksync-gui"),
                "enable_gui is set but mksync-gui is unavailable"
            )
            table.insert(arguments, "--gui")
            table.insert(arguments, guiTarget:targetfile())
        else
            table.insert(arguments, "--without-gui")
        end

        -- Invoke through Bash so packaging also works from workspaces mounted with noexec and
        -- after source-file replacement on filesystems that briefly report ETXTBSY.
        table.insert(arguments, 1, script)
        os.vrunv("bash", arguments)
    end)
target_end()
end
