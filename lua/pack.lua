includes("@builtin/xpack")

local package_basename = "mksync-$(version)-$(plat)-$(arch)-$(mode)"

xpack("mksync")
    set_title("mksync")
    set_author("Btk-Project")
    set_maintainer("Btk-Project")
    set_description("Mouse and keyboard synchronization tool")
    set_license("GPL-3.0-or-later")
    set_licensefile("LICENSE")
    set_basename(package_basename)
    add_targets("mksync")
    set_bindir("bin")
    set_libdir("lib")

    if is_plat("windows", "mingw") then
        set_formats("zip")
        add_installfiles(path.join(os.projectdir(), get_config("outputdir")) .. "/(**)|**.pdb|**.lib")
    else
        set_formats("targz")
        add_installfiles(path.join(os.projectdir(), get_config("outputdir")) .. "/(**)|**.sym|**.a")
    end
xpack_end()

if is_plat("windows", "mingw") or is_mode("releasedbg") then
xpack("test_debug_symbol")
    if is_plat("windows", "mingw") then
        set_formats("zip")
        add_installfiles(path.join(os.projectdir(), get_config("outputdir")) .. "/(**.pdb)", path.join(os.projectdir(), get_config("outputdir")) .. "/(**.lib)")
    else
        set_formats("targz")
        add_installfiles(path.join(os.projectdir(), get_config("outputdir")) .. "/(**.sym)", path.join(os.projectdir(), get_config("outputdir")) .. "/(**.a)")
    end
xpack_end()
end
