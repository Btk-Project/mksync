target("core")
    set_kind("$(kind)")
    set_targetdir("$(libdir)")

    add_deps("config", "proto", "base")
    add_packages("ilias", "neko-proto-tools", "spdlog", "cxxopts", "rapidjson")
    if is_plat("windows", "mingw") then
        add_syslinks("user32")
    elseif is_plat("linux") then
        add_packages("xcb", "x11")
    end

    add_includedirs("include", {public = true})
    add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")

    on_load(function (target) 
        import("lua.auto", {rootdir = os.projectdir()})
        auto().target_autoname(target)
        auto().library_autodefine(target)
    end)
    after_build(function (target) 
        import("lua.auto", {rootdir = os.projectdir()})
        auto().target_autoclean(target)
    end)
target_end()