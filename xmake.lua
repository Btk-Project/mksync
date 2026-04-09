-- project configuration
set_project("mksync")
set_version("0.0.1", {build = "$(buildversion)", soname = true})
set_xmakever("3.0.0")
option("alias", {showmenu = false, default = "mks"}) -- project abbreviation

set_configvar("LEGAL_COPYRIGHT", "Copyright (C) 2024 Btk-Project")
set_configvar("PROJECT_NAME", "mksync")

-- global configuration
option("stdc",   {showmenu = true, default = 23, values = {23, 17, 11, 99}})
option("stdcxx", {showmenu = true, default = 23, values = {26, 23, 17, 11}})
function stdc()   return "c"   .. tostring(get_config("stdc"))   end
function stdcxx() return "c++" .. tostring(get_config("stdcxx")) end

set_languages(stdc(), stdcxx())
set_warnings("allextra")
set_encodings("utf-8")
set_exceptions("cxx")

-- compile rules
add_rules("mode.release", "mode.debug")
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd", outputdir = ".vscode"})

-- compile configuration
option("3rd_custom",   {showmenu = true, type = "boolean", default = false})
option("3rd_kind",     {showmenu = true, type = "string",  default = get_config("kind"), values = {"static", "shared"}})
option("3rd_mode",     {showmenu = true, type = "string",  default = "release",          values = {"release", "debug"}})
option("outputdir",    {showmenu = true, type = "string",  default = path.join(os.projectdir(), "bin")})
option("buildversion", {showmenu = true, type = "number",  default = 0})

includes("lua/check")
check_macros("has_std_out_ptr",         "__cpp_lib_out_ptr",            {languages = stdcxx(), includes = "version"})
check_macros("has_std_expected",        "__cpp_lib_expected",           {languages = stdcxx(), includes = "version"})
check_macros("has_std_format",          "__cpp_lib_format >= 202207L",  {languages = stdcxx(), includes = "version"})

-- hide options, hide targets, pack targets
includes("lua/hideoptions.lua")
includes("lua/hidetargets.lua")
includes("lua/pack.lua")

-- some of the required libraries use our own repository
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
-- header-only libraries
if not has_config("has_std_out_ptr")  then add_requires("out_ptr") end
if not has_config("has_std_expected") then add_requires("zeus_expected") end
-- normal libraries
if not has_config("has_std_format") then add_requires("fmt") end
add_requires(
    "spdlog",
    "ilias"
)
if is_plat("linux") then
    add_requires(
        "libportal",
        "libx11",
        "libxcb",
        "libxi",
        "xcb-util-keysyms"
    )
    -- sudo apt install libx11-dev libxi-dev libxcb-keysyms1-dev libxcb-util0-dev libxcb-xtest0-dev
end


if not has_config("3rd_custom") then
-- configurations of required libraries
add_requireconfs("**out_ptr",       {override = true, version = "x.x.x"})
add_requireconfs("**zeus_expected", {override = true, version = "x.x.x"})
-- normal libraries' dependencies configurations
add_requireconfs("**fmt",       {override = true, version = "x.x.x", configs = {shared = is_config("3rd_kind", "shared"), debug = is_config("3rd_mode", "debug"), header_only = false}})
add_requireconfs("**spdlog",    {override = true, version = "x.x.x", configs = {shared = is_config("3rd_kind", "shared"), debug = is_config("3rd_mode", "debug"), header_only = false, fmt_external = not has_config("has_std_format"), std_format = has_config("has_std_format"), wchar = true, wchar_console = true}})
add_requireconfs("**ilias",     {override = true, version = "x.x.x", configs = {debug = is_config("3rd_mode", "debug")}})
-- configurations of dependency libraries
add_requireconfs("**libportal",         {system = true, optional = true})
add_requireconfs("**libx11",            {system = true})
add_requireconfs("**libxcb",            {system = true})
add_requireconfs("**libxi",             {system = true})
add_requireconfs("**xcb-util-keysyms",  {system = true})
    -- sudo apt install python3-pip libgirepository1.0-dev valac
end

-- subdirectories
includes("src/*/xmake.lua")
-- includes("exec/*/xmake.lua")
-- includes("test/*/xmake.lua")

target("mksync")
    set_kind("binary")
    add_packages("ilias")
    add_packages("spdlog")
    add_deps("config")
    if is_plat("windows") then 
        add_syslinks("user32")
    end
    
    add_includedirs("src")
    set_pcxxheader("src/pch.hpp")
    add_files("src/**.cpp")
target_end()