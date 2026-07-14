includes("@builtin/xpack")

local package_basename = "mksync-$(version)-$(plat)-$(arch)-$(mode)"

xpack("mksync")
    set_title("mksync")
    set_author("Btk-Project")
    set_maintainer("Btk-Project <btk-project@users.noreply.github.com>")
    set_description("Mouse and keyboard synchronization tool")
    set_homepage("https://github.com/Btk-Project/mksync")
    set_license("GPL-3.0-or-later")
    set_licensefile("LICENSE")
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

    before_package(function (package)
        if package:format() == "deb" then
            os.setenv("LC_ALL", "C")
            os.setenv("LC_TIME", "C")
            if has_config("enable_gui") then
                local template = path.join(os.programdir(), "scripts", "xpack", "deb", "debian")
                local specdir = path.join(package:builddir(), "debian-template")
                os.cp(template, specdir)
                local control = path.join(specdir, "control")
                io.replace(control, "Section: devel", "Section: utils", {plain = true})
                io.replace(
                    control,
                    "Depends: ${shlibs:Depends}, ${misc:Depends}",
                    "Depends: ${shlibs:Depends}, ${misc:Depends}, " ..
                        "qml6-module-qtquick, qml6-module-qtquick-controls, " ..
                        "qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts",
                    {plain = true}
                )
                package:set("specfile", specdir)
            end
        end
    end)

    if is_plat("windows", "mingw") then
        -- qt.quickapp's xpack install hook runs windeployqt for every binary format.
        set_formats("nsis", "zip")
    elseif is_plat("macosx") then
        -- qt.quickapp deploys the .app bundle before xpack creates these archives.
        set_formats("dmg", "zip")
    else
        -- Linux Qt and backend libraries remain system dependencies. deb/rpm record them
        -- through the native package tools; targz is a distro-oriented portable archive.
        set_formats("deb", "rpm", "targz")
    end
xpack_end()
