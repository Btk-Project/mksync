
if is_host("linux") and not is_plat("corss") then
add_requires("libx11", "libxi", "libxcb")
-- sudo apt install libx11-dev libxi-dev libxcb-keysyms1-dev
target("capture_x11")
    set_kind("binary")
    set_targetdir("$(testdir)")

    add_deps("proto")
    add_rules("targetclean")
    add_packages("out_ptr", "sobjectizer", "spdlog")
    add_packages("libx11", "libxi", "libxcb", "ilias")
    add_links("X11", "Xi", "xcb", "xcb-keysyms", "Xtst")

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