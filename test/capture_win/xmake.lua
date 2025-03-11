

if is_plat("windows", "mingw") then
target("capture_win")
    set_kind("binary")
    set_targetdir("$(testdir)")

    add_deps("base")
    add_defines("ILIAS_ENABLE_LOG")
    add_defines("NEKO_PROTO_LOG_CONTEXT")
    add_rules("target.clean")
    add_packages("out_ptr", "sobjectizer", "spdlog", "ilias", "neko-proto", "cxxopts")
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