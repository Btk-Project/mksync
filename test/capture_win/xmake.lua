

if is_host("windows") and not is_plat("corss") then
target("capture_win")
    set_kind("binary")
    set_targetdir("$(testdir)")

    add_deps("base")
    add_defines("ILIAS_ENABLE_LOG")
    add_defines("NEKO_PROTO_LOG_CONTEXT")
    add_rules("targetclean")
    add_packages("out_ptr", "sobjectizer", "spdlog", "ilias", "neko-proto")
    add_links("user32")

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