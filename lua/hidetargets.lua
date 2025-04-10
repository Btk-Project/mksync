

target("export")
    set_default(false)
    set_kind("phony")
    set_targetdir("$(libdir)")

    after_build(function (target)
        import("core.project.project")
        cprint("${yellow}Export Shared libs")
        for pkg_name, info in pairs(project:required_packages()) do
            local libfiles = info:get("libfiles")
            local sharedcnt = 0;
            for i, filepath in ipairs(libfiles) do
                if path.extension(filepath) == ".a" or path.extension(filepath) == ".lib" then
                else
                    sharedcnt = sharedcnt + 1;
                end
            end
            cprint("${yellow}" .. pkg_name .. "${clear} lib count=" .. format("%d", sharedcnt) .. ":")
            for i, filepath in ipairs(libfiles) do
                if path.extension(filepath) == ".a" or path.extension(filepath) == ".lib" then
                else
                    print("  " .. filepath .. " -> " .. path.translate(target:targetdir()))
                    os.tryrm(path.join(path.translate(target:targetdir()), path.filename(filepath)))
                    os.cp(filepath, target:targetdir() .. "/", {symlink = true})
                end
            end
        end
    end)
target_end()