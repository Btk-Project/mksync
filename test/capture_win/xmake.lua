

if is_host("windows") and not is_plat("corss") then
target("capture_win")
    set_kind("binary")
    set_targetdir("$(testdir)")

    add_deps("proto")
    add_rules("targetclean")
    add_packages("out_ptr", "sobjectizer", "spdlog")

    -- version
    -- set_configdir("./")
    -- add_configfiles("*.rc.in")
    -- add_files("*.rc")
    -- target files
    -- add_includedirs("include", {public = true})
    -- add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")
target_end()
end