function config_number(name)
    local value = get_config(name)
    if type(value) == "number" then
        return value
    end
    return tonumber(value)
end

function stdc_version()
    return config_number("stdc")
end

function stdcxx_version()
    return config_number("stdcxx")
end

function stdc()
    return "c" .. tostring(stdc_version())
end

function stdcxx()
    return "c++" .. tostring(stdcxx_version())
end

function check_system_pkgconfig_package(name, packageNames)
    if type(packageNames) == "string" then
        packageNames = {packageNames}
    end
    interp_save_scope()
    option(name)
        set_showmenu(false)
        on_check(function(option)
            for _, packageName in ipairs(packageNames) do
                local ok = try {
                    function()
                        os.vrunv("pkg-config", {"--exists", packageName})
                        return true
                    end
                }
                if ok then
                    option:enable(true)
                    return
                end
            end
        end)
    option_end()
    interp_restore_scope()
    add_options(name)
end

function read_os_release()
    local values = {}
    local file = io.open("/etc/os-release", "r")
    if not file then
        return values
    end
    for line in file:lines() do
        local key, value = line:match("^([%w_]+)=(.*)$")
        if key and value then
            value = value:gsub('^"', ""):gsub('"$', "")
            values[key] = value
        end
    end
    file:close()
    return values
end

function os_release_matches(values, ...)
    local ids = table.pack(...)
    local osIds = " " .. (values.ID or "") .. " " .. (values.ID_LIKE or "") .. " "
    for i = 1, ids.n do
        if osIds:find(" " .. ids[i] .. " ", 1, true) then
            return true
        end
    end
    return false
end

function system_fmt_missing_message()
    local hint = "install the package that provides fmt.pc, or disable system spdlog"
    if is_plat("linux") then
        local osRelease = read_os_release()
        if os_release_matches(osRelease, "debian", "ubuntu", "uos", "kylin") then
            hint = "run: sudo apt install libfmt-dev"
        elseif os_release_matches(osRelease, "fedora", "rhel", "centos", "rocky", "almalinux") then
            hint = "run: sudo dnf install fmt-devel"
        elseif os_release_matches(osRelease, "arch", "manjaro") then
            hint = "run: sudo pacman -S fmt"
        elseif os_release_matches(osRelease, "opensuse", "suse") then
            hint = "run: sudo zypper install fmt-devel"
        elseif os_release_matches(osRelease, "alpine") then
            hint = "run: sudo apk add fmt-dev"
        end
    end
    return "system spdlog was found, but system fmt was not found; " .. hint
end
