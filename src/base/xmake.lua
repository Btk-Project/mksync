

target("base")
    set_kind("$(kind)")
    set_targetdir("$(libdir)")

    add_deps("proto")

    add_rules("target.clean", "target.autoname", "library.autodefine")

    -- version
    -- set_configdir("./")
    -- add_configfiles("*.rc.in")
    -- add_files("*.rc")
    -- target files
    add_includedirs("include", {public = true})
    add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")

    add_packages("ilias", "neko-proto", "spdlog")
    if is_plat("windows", "mingw") then
        add_syslinks("user32")
    elseif is_plat("linux") then
        add_links("xcb", "xcb-keysyms", "xcb-util", "xcb-xtest", "xcb-composite", "xcb-render", "X11", "anl")
        -- sudo apt install libxcb-util-dev libxcb-xtest0-dev libxcb-keysyms1-dev libx11-dev libxcb-composite0-dev libxcb-render0-dev
    end
target_end()