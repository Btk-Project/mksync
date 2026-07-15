local function backend_option(name, description)
    option(name)
        set_default(true)
        set_showmenu(true)
        set_description(description)
        set_category("mksync backends")
    option_end()
end

backend_option("enable_backend_x11", "Build the Linux X11/XCB backend")
backend_option("enable_backend_wayland_wlr", "Build the Linux wlroots Wayland backend")
backend_option("enable_backend_wayland_portal", "Build the Linux portal/libei backend")
backend_option("enable_backend_win32", "Build the native Windows backend")

if is_plat("linux") and has_config("enable_backend_x11") then
    add_requires("pkgconfig::xcb", {system = true})
    add_requires("pkgconfig::xcb-keysyms", {system = true})
    add_requires("pkgconfig::xcb-randr", {system = true})
    add_requires("pkgconfig::xcb-xinput", {system = true})
    add_requires("pkgconfig::xcb-xtest", {system = true})
end

if is_plat("linux") and has_config("enable_backend_wayland_wlr") then
    add_requires("pkgconfig::wayland-client", {system = true})
    add_requires("pkgconfig::wayland-protocols", {system = true})
    add_requires("pkgconfig::xkbcommon", {system = true})
end

if is_plat("linux") and has_config("enable_backend_wayland_portal") then
    add_requires("pkgconfig::libei-1.0", {system = true})
    add_requires("pkgconfig::libportal >=0.8.0", {system = true})
end

function mks_add_backend_packages()
    if is_plat("linux") and has_config("enable_backend_x11") then
        add_packages("pkgconfig::xcb")
        add_packages("pkgconfig::xcb-keysyms")
        add_packages("pkgconfig::xcb-randr")
        add_packages("pkgconfig::xcb-xinput")
        add_packages("pkgconfig::xcb-xtest")
    end

    if is_plat("linux") and has_config("enable_backend_wayland_wlr") then
        add_packages("pkgconfig::wayland-client")
        add_packages("pkgconfig::wayland-protocols")
        add_packages("pkgconfig::xkbcommon")
        add_rules("mks.wayland_client_protocol")
        add_files(path.join(os.projectdir(), "protocols/*.xml"))
    end

    if is_plat("linux") and has_config("enable_backend_wayland_portal") then
        add_packages("pkgconfig::libei-1.0")
        add_packages("pkgconfig::libportal")
    end

    if is_plat("windows") and has_config("enable_backend_win32") then
        add_syslinks("user32", "shcore")
    end
end

function mks_add_backend_sources()
    local platform_dir = path.join(os.projectdir(), "src", "platform")
    add_files(path.join(platform_dir, "backend.cpp"))
    add_files(path.join(platform_dir, "platform.cpp"))

    if is_plat("linux") and has_config("enable_backend_x11") then
        add_files(path.join(platform_dir, "xcb.cpp"))
        add_files(path.join(platform_dir, "xcb_connection.cpp"))
    end
    if is_plat("linux") and has_config("enable_backend_wayland_wlr") then
        add_files(path.join(platform_dir, "wayland.cpp"))
    end
    if is_plat("linux") and has_config("enable_backend_wayland_portal") then
        add_files(path.join(platform_dir, "wayland_portal.cpp"))
    end
    if is_plat("windows") and has_config("enable_backend_win32") then
        add_files(path.join(platform_dir, "win32.cpp"))
    end
end
