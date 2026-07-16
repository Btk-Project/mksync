includes("@builtin/xpack")

local package_basename = "mksync-$(version)-$(plat)-$(arch)-$(mode)"

xpack("mksync")
    set_title("mksync")
    set_author("Btk-Project")
    set_maintainer("Btk-Project <btk-project@users.noreply.github.com>")
    set_description("Mouse and keyboard synchronization tool")
    set_homepage("https://github.com/Btk-Project/mksync")
    set_license("GPL-3.0-or-later")
    set_licensefile("../LICENSE")
    -- Release packages consume the already configured targets. This keeps optional backends and
    -- the optional Qt target identical between the build and the package.
    set_inputkind("binary")
    set_basename(package_basename)
    add_targets("mksync")
    if has_config("enable_gui") then
        add_targets("mksync-gui")
    end
    add_installfiles("LICENSE", "README.md", "README_zh.md",
                     {prefixdir = "share/doc/mksync"})
    if is_plat("windows", "mingw") then
        -- windeployqt places the GUI and its runtime at the package root.
        set_bindir(".")
    else
        set_bindir("bin")
    end
    set_libdir("lib")
    -- add_sourcefiles(
    --     "xmake.lua",
    --     "lua/**",
    --     "src/**",
    --     "tests/xmake.lua",
    --     "LICENSE"
    -- )

    if is_plat("windows", "mingw") then
        -- qt.quickapp's xpack install hook runs windeployqt for every binary format.
        set_formats("nsis", "zip")
    elseif is_plat("macosx") then
        -- qt.quickapp deploys the .app bundle before xpack creates these archives.
        set_formats("dmg", "zip")
    else
        -- Debian packages use the package_deb phony target and packaging/debian/build.sh. Xpack's
        -- Debian backend creates a source package and starts a second build whose Xmake dependency
        -- cannot be satisfied by the distribution's older Xmake package.
        set_formats("rpm", "targz")
    end
xpack_end()
