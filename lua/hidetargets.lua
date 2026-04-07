

target("export")
    set_default(false)
    set_kind("phony")
    set_targetdir(get_config("libdir"))

    after_build(function (target)
        import("core.project.project")
        cprint("${yellow}Export Shared libs${clear} -> %s", path.translate(target:targetdir()))
        -- insert
        local filepaths = {}
        for pkg_name, info in pairs(project:required_packages()) do
            for _, filepath in ipairs(info:get("libfiles")) do
                local flag = false
                repeat
                    if filepaths[filepath] ~= nil then
                        flag = true
                    else
                        filepaths[filepath] = pkg_name
                        if os.islink(filepath) then
                            filepath = path.join(path.directory(filepath), os.readlink(filepath))
                        end
                    end
                until flag == true
            end
        end
        -- find links
        local exportfiles = {}
        for filepath, pkg_name in pairs(filepaths) do
            if not (exportfiles[pkg_name] ~= nil) then
                exportfiles[pkg_name] = {}
            end
            if path.extension(filepath) == ".a" or path.extension(filepath) == ".lib" then
            else
                table.insert(exportfiles[pkg_name], filepath)
            end
        end
        -- export
        for pkg_name, files in pairs(exportfiles) do
            table.sort(files)
            print(pkg_name, "(lib count: " .. tostring(#files) .. ")", files)
            for _, file in ipairs(files) do
                local targetpath = path.join(path.translate(target:targetdir()), path.filename(file))
                if os.isfile(targetpath) and hash.sha256(file) ~= hash.sha256(targetpath) then
                    os.tryrm(targetpath)
                    os.cp(file, target:targetdir() .. "/", {symlink = true})
                elseif not os.isfile(targetpath) then
                    os.cp(file, target:targetdir() .. "/", {symlink = true})
                end
            end
        end
    end)
target_end()
