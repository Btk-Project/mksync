

option("libdir")
    add_deps("outputdir")
    set_showmenu(false)
    on_check(function (option)
        option:set_value(path.translate(
            vformat("$(outputdir)/lib/$(arch)-$(kind)-$(runtimes)", xmake)
        ))
    end)
option_end()

option("bindir")
    add_deps("outputdir")
    set_showmenu(false)
    on_check(function (option)
        option:set_value(path.translate(
            vformat("$(outputdir)/bin/$(arch)-$(kind)-$(runtimes)", xmake)
        ))
    end)
option_end()

option("testdir")
    add_deps("outputdir")
    set_showmenu(false)
    on_check(function (option)
        option:set_value(path.translate(
            vformat("$(outputdir)/test/$(arch)-$(kind)-$(runtimes)", xmake)
        ))
    end)
option_end()

option("plugindir")
    add_deps("outputdir")
    set_showmenu(false)
    on_check(function (option)
        option:set_value(path.translate(
            vformat("$(outputdir)/plugin/$(arch)-$(kind)-$(runtimes)", xmake)
        ))
    end)
option_end()

option("datadir")
    add_deps("outputdir")
    set_showmenu(false)
    on_check(function (option)
        option:set_value(path.translate(
            vformat("$(outputdir)/data", xmake)
        ))
    end)
option_end()

rule("mode.mydebug")
    on_config(function (target)

        if is_mode("mydebug") then
            if not target:get("symbols") then
                target:set("symbols", "debug")
            end
            if not target:get("optimize") then
                target:set("optimize", "none")
            end
            target:add("cuflags", "-G")
        end
    end)