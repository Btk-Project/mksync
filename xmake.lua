set_project("mksync")
set_version("0.0.1", {build = "$(buildversion)"})

set_configvar("LEGAL_COPYRIGHT", "Copyright (C) 2024 Btk-Project")
set_configvar("PROJECT_NAME", "mksync")

set_warnings("allextra")
set_languages("cxx20", "c17")
set_exceptions("cxx")
set_encodings("utf-8")

add_rules("mode.release", "mode.debug", "mode.releasedbg", "mode.minsizerel")
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd", outputdir = ".vscode"})

if is_plat("linux") then
    add_cxxflags("-fcoroutines")
    add_links("rt")
end

rule("targetclean")
    after_build(function (target)
        os.tryrm(target:targetdir() .. "/" .. target:basename() .. ".exp")
        os.tryrm(target:targetdir() .. "/" .. target:basename() .. ".ilk")
        os.tryrm(target:targetdir() .. "/compile." .. target:basename() .. ".pdb")
        if target:kind() == "static" then
        elseif target:kind() == "shared" then
        elseif target:kind() == "binary" then
        end
    end)
rule_end()

option("3rd_kind")
    set_showmenu(true)
    set_default("shared")
    set_values("static", "shared")
option_end()

option("buildversion")
    set_showmenu(true)
    set_default("0")
option_end()

option("outputdir")
    set_showmenu(true)
    on_check(function (option)
        if not option:value() then
            option:set_value(path.translate(vformat("$(projectdir)/bin/$(arch)-$(kind)-$(runtimes)", xmake)))
        end
    end)
option_end()

includes("lua/hideoptions.lua")

-- Some of the third-libraries use our own configurations
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
-- headonly
add_requires(
    -- c++23
    "out_ptr",
    -- tools
    "cxxopts",
    "rapidjson"
)
add_requires("ilias dev")
-- Use dynamic libraries for direct dependencies and static libraries for indirect dependencies.
add_requires("fmt", {configs = {shared = is_config("3rd_kind", "shared"), header_only = false}})
add_requires("spdlog", {configs = {shared = is_config("3rd_kind", "shared"), header_only = false, fmt_external = true, wchar = true, wchar_filename = false, console_wchar = true}})
add_requires("sobjectizer", {configs = {shared = is_config("3rd_kind", "shared")}})
add_requires("neko-proto dev", {configs = {shared = is_config("3rd_kind", "shared"), enable_simdjson = false, enable_rapidjson = true, enable_fmt = true, enable_communication = true}})
if is_plat("linux") then
    add_requires("libportal", {configs = {shared = is_config("3rd_kind", "shared")}})
end
-- third party dependencies settings
add_requireconfs("**.fmt", {configs = {shared = is_config("3rd_kind", "shared"), header_only = false}})
add_requireconfs("**.spdlog", {configs = {shared = is_config("3rd_kind", "shared"), header_only = false, fmt_external = true, wchar = true, wchar_filename = false, console_wchar = true}})
add_requireconfs("**.sobjectizer", {configs = {shared = is_config("3rd_kind", "shared")}})
add_requireconfs("**.neko-proto", {version = "dev", override = true, configs = {shared = is_config("3rd_kind", "shared"), enable_simdjson = false, enable_rapidjson = true, enable_fmt = true, enable_communication = true}})
add_requireconfs("**.ilias", {version = "dev", override = true})
if is_plat("linux") then
    add_requireconfs("**.libportal", {configs = {shared = is_config("3rd_kind", "shared")}})
    -- apt::python3-pip apt::libgirepository1.0-dev apt::valac
    -- sudo apt install python3-pip libgirepository1.0-dev valac
end

includes("src/*/xmake.lua")
includes("exec/*/xmake.lua")
includes("test/*/xmake.lua")
