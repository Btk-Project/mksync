

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
target_end()