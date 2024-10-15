package("ilias")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/BusyStudent/Ilias")
    set_description("ilias, header-only network library based on cpp20 coroutine")

    add_urls("https://github.com/BusyStudent/Ilias.git")
    -- add_versions("0.1", "0f490d124753f63f42d28a8fe08f48a59ce56191")

    if is_host("windows") and not is_plat("cross") then
        add_syslinks("Ws2_32")
    end

    on_install(function (package)
        local configs = {}
        import("package.tools.xmake").install(package, configs)
    end)

    on_test(function (package)
        -- TODO check includes and interfaces
        -- assert(package:has_cfuncs("foo", {includes = "foo.h"})
    end)