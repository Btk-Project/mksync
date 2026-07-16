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

function mks_spdlog_package()
    return mks_uses_system_spdlog() and "pkgconfig::spdlog" or "spdlog"
end

function mks_fmt_package()
    return has_config("has_system_fmt") and "pkgconfig::fmt" or "fmt"
end

function mks_requires_fmt()
    return mks_uses_system_spdlog() or not has_config("has_std_format")
end

function mks_uses_system_spdlog()
    return has_config("has_system_fmt") and has_config("has_system_spdlog")
end
