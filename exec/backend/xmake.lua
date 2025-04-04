if is_plat("linux") then 
    option("memcheck")
        set_default(false)
        set_showmenu(true)
    option_end()
end 

target("backend")
    set_kind("binary")
    set_targetdir("$(bindir)")

    add_deps("config", "core")
    add_packages("cxxopts", "ilias", "sobjectizer", "spdlog", "neko-proto-tools")

    -- add_includedirs("include", {public = true})
    -- add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")

    if has_config("memcheck") then
        on_run(function (target)
            local argv = {}
            table.insert(argv, "--leak-check=full")
            table.insert(argv, "--track-origins=yes")
            table.insert(argv, target:targetfile())
            os.execv("valgrind", argv)
        end)
    end

    after_build(function (target) 
        import("lua.auto", {rootdir = os.projectdir()})
        auto().target_autoclean(target)
        -- auto().binary_autoluanch(target)
    end)
target_end()