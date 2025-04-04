

if is_plat("windows", "mingw") then
target("capture_win")
    set_kind("binary")
    set_default(false)
    set_targetdir("$(testdir)")

    add_deps("config", "base", "core")
    add_packages("out_ptr", "sobjectizer", "spdlog", "ilias", "neko-proto-tools", "cxxopts")
    add_defines("NEKO_PROTO_LOG_CONTEXT")
    add_links("user32")

    -- add_includedirs("include", {public = true})
    -- add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")

    after_build(function (target) 
        import("lua.auto", {rootdir = os.projectdir()})
        auto().target_autoclean(target)
        -- auto().binary_autoluanch(target)
    end)
target_end()
end