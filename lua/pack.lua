includes("@builtin/xpack")

xpack("test")
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
