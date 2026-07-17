-- Qt application staging for Xpack. Windows formats deliberately have different policies:
-- the installer assumes the normal Microsoft/system graphics runtimes, while the portable ZIP
-- keeps windeployqt's complete runtime set.

import("rules.qt.install.windeployqt", {rootdir = os.programdir()})
import("plugins.pack.batchcmds", {
    alias = "pack_batchcmds",
    rootdir = os.programdir()
})

local function appendUnique(values, value)
    if not table.contains(values, value) then
        table.insert(values, value)
    end
end

local function pruneInstallerRuntime(deploydir)
    -- mksync fixes Quick Controls to Basic. Keep every dependency of that style, but do not ship
    -- the alternative style engines or QML debugger tooling in the installed edition.
    local unusedStyles = {
        "FluentWinUI3",
        "Fusion",
        "Imagine",
        "Material",
        "Universal",
        "Windows"
    }
    for _, style in ipairs(unusedStyles) do
        os.tryrm(path.join(deploydir, "qml", "QtQuick", "Controls", style))
    end
    os.tryrm(path.join(deploydir, "qmltooling"))

    local unusedStyleLibraries = {
        "Qt6QuickControls2FluentWinUI3StyleImpl.dll",
        "Qt6QuickControls2Fusion.dll",
        "Qt6QuickControls2FusionStyleImpl.dll",
        "Qt6QuickControls2Imagine.dll",
        "Qt6QuickControls2ImagineStyleImpl.dll",
        "Qt6QuickControls2Material.dll",
        "Qt6QuickControls2MaterialStyleImpl.dll",
        "Qt6QuickControls2Universal.dll",
        "Qt6QuickControls2UniversalStyleImpl.dll",
        "Qt6QuickControls2WindowsStyleImpl.dll"
    }
    for _, library in ipairs(unusedStyleLibraries) do
        os.tryrm(path.join(deploydir, library))
    end
end

local function deployWindows(target, batchcmds, opt)
    local package = opt.package
    local installdir = package:installdir()
    local format = package:format()
    local deploydir = path.join(target:autogendir(), "qt", "deploy", target:name(), format)
    os.tryrm(deploydir)
    os.mkdir(deploydir)

    local targetfile = path.join(deploydir, target:filename())
    os.cp(target:targetfile(), targetfile)
    local installfiles = {targetfile}
    for _, dep in ipairs(target:orderdeps()) do
        if dep:rule("qt.shared") then
            local depfile = path.join(deploydir, path.filename(dep:targetfile()))
            os.cp(dep:targetfile(), depfile)
            table.insert(installfiles, depfile)
        end
    end

    local originalFlags = target:values("qt.deploy.flags")
    local deployFlags = table.copy(table.wrap(originalFlags))
    if format == "nsis" then
        appendUnique(deployFlags, "--no-opengl-sw")
        appendUnique(deployFlags, "--no-system-d3d-compiler")
        if target:is_plat("windows") then
            appendUnique(deployFlags, "--no-compiler-runtime")
        end
    end
    target:values_set("qt.deploy.flags", table.unpack(deployFlags))
    windeployqt.run_deploy(target, deploydir, installfiles)
    target:values_set("qt.deploy.flags", table.unpack(table.wrap(originalFlags)))
    if format == "nsis" then
        pruneInstallerRuntime(deploydir)
    end

    batchcmds:mkdir(installdir)
    batchcmds:cp(path.join(deploydir, "*"), installdir, {rootdir = deploydir})
    pack_batchcmds.install_target_shared_libraries(target, batchcmds, {
        bindir = installdir,
        package = package
    })
end

function main(target, batchcmds, opt)
    local package = opt.package
    if not package or not package:installdir() then
        return
    end

    if target:is_plat("macosx") then
        local targetApp = path.join(target:targetdir(), target:basename() .. ".app")
        if os.isdir(targetApp) then
            batchcmds:cp(targetApp, path.join(package:installdir(), path.filename(targetApp)), {
                symlink = true
            })
        end
    elseif target:is_plat("windows", "mingw") then
        deployWindows(target, batchcmds, opt)
    else
        local bindir = target:bindir()
        if bindir and os.isdir(bindir) then
            batchcmds:cp(path.join(bindir, "*"), package:installdir("bin"), {rootdir = bindir})
            pack_batchcmds.install_target_shared_libraries(target, batchcmds, {
                bindir = package:installdir("lib"),
                package = package
            })
        end
    end

    if not target:is_plat("macosx") then
        pack_batchcmds.install_target_files(target, batchcmds, opt)
        pack_batchcmds.update_target_install_rpath(target, batchcmds, opt)
    end
end
