-- Uninstall counterpart of lua/qt_installcmd.lua. Keep the generated NSIS removal commands in
-- lockstep with the format-specific windeployqt staging directory used during installation.

import("private.utils.target", {
    alias = "target_utils",
    rootdir = os.programdir()
})

local function removeTargetInstallFiles(target, batchcmds, package)
    local installdir = package:installdir()
    local _, dstfiles = target:installfiles(installdir)
    for _, dstfile in ipairs(dstfiles or {}) do
        batchcmds:rm(dstfile, {emptydirs = true})
    end
    for _, dep in ipairs(target:orderdeps()) do
        _, dstfiles = dep:installfiles(installdir, {interface = true})
        for _, dstfile in ipairs(dstfiles or {}) do
            batchcmds:rm(dstfile, {emptydirs = true})
        end
    end
end

local function removeTargetSharedLibraries(target, batchcmds, package)
    local libfiles = {}
    target_utils.get_target_libfiles(target, libfiles, target:targetfile(), {})
    for _, libfile in ipairs(table.unique(libfiles)) do
        batchcmds:rm(path.join(package:installdir(), path.filename(libfile)), {
            emptydirs = true
        })
    end
end

function main(target, batchcmds, opt)
    local package = opt.package
    if not package or not package:installdir() then
        return
    end

    local installdir = package:installdir()
    local format = package:format()
    local deploydir = path.join(target:autogendir(), "qt", "deploy", target:name(), format)
    if os.isdir(deploydir) then
        for _, item in ipairs(os.filedirs(path.join(deploydir, "*"))) do
            local destination = path.join(installdir, path.relative(item, deploydir))
            if os.isdir(item) then
                -- Only remove directories created by windeployqt. Never recursively remove the
                -- installation root because the user may have selected a non-empty directory.
                batchcmds:rmdir(destination, {emptydirs = true})
            else
                batchcmds:rm(destination, {emptydirs = true})
            end
        end
    else
        -- The directory normally exists while Xpack generates the NSIS script. Keep a narrow
        -- fallback so a missing staging directory cannot produce another empty uninstall section.
        batchcmds:rm(path.join(installdir, target:filename()), {emptydirs = true})
        for _, dep in ipairs(target:orderdeps()) do
            if dep:rule("qt.shared") then
                batchcmds:rm(path.join(installdir, path.filename(dep:targetfile())), {
                    emptydirs = true
                })
            end
        end
    end

    removeTargetSharedLibraries(target, batchcmds, package)
    removeTargetInstallFiles(target, batchcmds, package)
end
