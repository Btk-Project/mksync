

target("backend")
    set_kind("binary")
    set_targetdir("$(bindir)")

    on_load(function (target)
        function Camel(str)
            return str:sub(1, 1):upper() .. str:sub(2)
        end
        import("core.project.project")
        target:set("basename", Camel(project:name()) .. Camel(target:name()))
    end)

    add_deps("proto")
    add_rules("targetclean")

    add_packages("cxxopts", "ilias", "sobjectizer", "spdlog")

    -- version
    -- set_configdir("./")
    -- add_configfiles("*.rc.in")
    -- add_files("*.rc")
    -- target files
    -- add_includedirs("include", {public = true})
    -- add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")
target_end()