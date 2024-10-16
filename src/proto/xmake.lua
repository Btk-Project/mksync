

target("Proto")
    set_kind("$(kind)")
    set_targetdir("$(libdir)")

    on_load(function (target)
        function Camel(str)
            return str:sub(1, 1):upper() .. str:sub(2)
        end
        import("core.project.project")
        target:set("basename", Camel(project:name()) .. Camel(target:name()))
    end)

    add_deps("Base")
    add_defines("MKS_PROTO_EXPORTS")
    add_rules("targetclean")

    -- version
    -- set_configdir("./")
    -- add_configfiles("*.rc.in")
    -- add_files("*.rc")
    -- target files
    add_includedirs("include", {public = true})
    add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")
target_end()