-- 工程设置
set_project("mksync")
set_version("0.0.1", {build = "$(buildversion)"})

set_configvar("LEGAL_COPYRIGHT", "Copyright (C) 2024 Btk-Project")
set_configvar("PROJECT_NAME", "mksync")

-- 全局设置
set_warnings("allextra")
set_languages("cxx20", "c17")
set_exceptions("cxx")
set_encodings("utf-8")

-- 添加编译选项
add_rules("mode.release", "mode.debug", "mode.releasedbg", "mode.minsizerel")
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd", outputdir = ".vscode"})

if is_plat("linux") then
    add_cxxflags("-fcoroutines")
    add_links("rt")
end

-- 编译设置
option("3rd_kind",     {showmenu = true, default = "shared", values = {"static", "shared"}})
option("buildversion", {showmenu = true, default = "0", type = "string"})
option("outputdir",    {showmenu = true, default = "bin", type = "string"})

-- 隐藏设置、隐藏目标、打包命令
includes("lua/hideoptions.lua")
includes("lua/hidetargets.lua")
includes("lua/pack.lua")

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
add_requires("ilias", {version = "dev"})
-- normal libraries
add_requires("spdlog", {version = "1.x.x", configs = {shared = is_config("3rd_kind", "shared"), header_only = false, fmt_external = true, wchar = true, wchar_console = true}})
add_requires("neko-proto-tools", {version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_simdjson = false, enable_rapidjson = true, enable_fmt = true, enable_communication = true}})
if is_plat("linux") then
    add_requires("libportal", {system = true, optional = true})
    add_requires("libx11", {system = true})
    add_requires("libxcb", {system = true})
    add_requires("libxi", {system = true})
    -- sudo apt install libx11-dev libxi-dev libxcb-keysyms1-dev libxcb-util0-dev libxcb-xtest0-dev
end
-- normal libraries' dependencies configurations
add_requireconfs("**.fmt", {override = true, version = "11.0.x", configs = {shared = is_config("3rd_kind", "shared"), header_only = false}})
add_requireconfs("**.spdlog", {override = true, version = "1.x.x", configs = {shared = is_config("3rd_kind", "shared"), header_only = false, fmt_external = true, wchar = true, wchar_console = true}})
add_requireconfs("**.neko-proto-tools", {override = true, version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_simdjson = false, enable_rapidjson = true, enable_fmt = true, enable_communication = true}})
add_requireconfs("**.ilias", {override = true, version = "dev"})
if is_plat("linux") then
    add_requireconfs("**.libportal", {system = true, optional = true})
    -- apt::python3-pip apt::libgirepository1.0-dev apt::valac
    -- sudo apt install python3-pip libgirepository1.0-dev valac
end

if is_mode("debug") then
    add_defines("ILIAS_ENABLE_LOG", "ILIAS_USE_FMT")
    -- add_defines("ILIAS_USE_IO_URING")
    -- add_syslinks("rt", "uring")
    -- add_packages("liburing")
end

includes("src/*/xmake.lua")
-- includes("exec/*/xmake.lua")
-- includes("test/*/xmake.lua")
