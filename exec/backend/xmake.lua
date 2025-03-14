if is_plat("linux") then 
    option("memcheck")
        set_default(false)
        set_showmenu(true)
    option_end()
end 

target("backend")
    set_kind("binary")
    set_targetdir("$(bindir)")

    add_deps("core")
    add_rules("target.clean", "target.autoname")

    add_packages("cxxopts", "ilias", "sobjectizer", "spdlog", "neko-proto")

    -- version
    -- set_configdir("./")
    -- add_configfiles("*.rc.in")
    -- add_files("*.rc")
    -- target files
    -- add_includedirs("include", {public = true})
    -- add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")
    if has_config("memcheck") then
        on_run(function (target)
            local argv = {}
            table.insert(argv, "--leak-check=full")
            table.insert(argv, target:targetfile())
            os.execv("valgrind", argv)
        end)
    end 
target_end()