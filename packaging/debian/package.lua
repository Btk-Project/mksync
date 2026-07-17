local progress = import("utils.progress")
local project = import("core.project.project")

function main(opt)
    opt = opt or {}
    local cliTarget = assert(project.target("mksync"), "mksync target is unavailable")
    local iliasPackage = assert(cliTarget:pkg("ilias"), "mksync has no ilias package")
    local iliasDir = assert(iliasPackage:installdir(), "ilias package has no install directory")
    local script = path.join(os.projectdir(), "packaging", "debian", "build.sh")
    local outputdir = path.absolute(opt.outputdir or path.join(os.projectdir(), "dist"))
    local arguments = {
        "--cli", cliTarget:targetfile(),
        "--private-library-dir", path.join(iliasDir, "lib"),
        "--output-dir", outputdir
    }

    progress.show(0, "${color.build.target}packaging.deb -> %s", outputdir)

    if opt.enableGui then
        local guiTarget = assert(
            project.target("mksync-gui"),
            "enable_gui is set but mksync-gui is unavailable"
        )
        table.insert(arguments, "--gui")
        table.insert(arguments, guiTarget:targetfile())
        if opt.bundleQt then
            local qt = assert(guiTarget:data("qt"), "mksync-gui has no configured Qt SDK")
            table.insert(arguments, "--bundle-qt")
            table.insert(arguments, "--qt-library-dir")
            table.insert(arguments, assert(qt.libdir, "Qt SDK has no library directory"))
            table.insert(arguments, "--qt-plugin-dir")
            table.insert(arguments, assert(qt.pluginsdir, "Qt SDK has no plugin directory"))
            table.insert(arguments, "--qt-qml-dir")
            table.insert(arguments, assert(qt.qmldir, "Qt SDK has no QML directory"))
        end
    else
        table.insert(arguments, "--without-gui")
    end

    -- Invoke through Bash so packaging also works from workspaces mounted with noexec and after
    -- source-file replacement on filesystems that briefly report ETXTBSY. execv keeps packaging
    -- progress visible; vrunv suppresses successful subprocess output unless Xmake is verbose.
    table.insert(arguments, 1, script)
    os.execv("bash", arguments)
end
