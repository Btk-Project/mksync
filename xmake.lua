-- project configuration
set_project("mksync")
set_version("0.0.1", {build = "$(buildversion)", soname = true})
set_xmakever("3.0.9")
option("alias", {showmenu = false, default = "mks"}) -- project abbreviation

set_configvar("LEGAL_COPYRIGHT", "Copyright (C) 2024 Btk-Project")
set_configvar("PROJECT_NAME", "mksync")

-- global configuration
option("stdc",   {showmenu = true, default = 23, values = {23, 17, 11, 99}})
option("stdcxx", {showmenu = true, default = 23, values = {26, 23}})
includes("lua/config_helpers.lua")

set_languages(stdc(), stdcxx())
set_warnings("allextra")
set_encodings("utf-8")
set_exceptions("cxx")
add_defines("SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE")

-- compile rules
add_rules("mode.release", "mode.debug")
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd", outputdir = ".vscode"})
includes("lua/wayland_protocols.lua")

-- compile configuration
option("3rd_custom",   {showmenu = true, type = "boolean", default = false})
option("3rd_kind",     {showmenu = true, type = "string",  default = get_config("kind"), values = {"static", "shared"}})
option("3rd_mode",     {showmenu = true, type = "string",  default = "release",          values = {"release", "debug"}})
option("outputdir",    {showmenu = true, type = "string",  default = path.join(os.projectdir(), "bin")})
option("buildversion", {showmenu = true, type = "number",  default = 0})
option("enable_gui")
    set_default(false)
    set_showmenu(true)
    set_description("Enable the isolated Qt 6/QML GUI target")
    set_category("mksync features")
option_end()

includes("lua/check")
check_macros("has_std_out_ptr",         "__cpp_lib_out_ptr",            {languages = stdcxx(), includes = "version"})
check_macros("has_std_expected",        "__cpp_lib_expected",           {languages = stdcxx(), includes = "version"})
check_macros("has_std_format",          "__cpp_lib_format >= 202207L",  {languages = stdcxx(), includes = "version"})
check_system_pkgconfig_package("has_system_fmt",              "fmt")
check_system_pkgconfig_package("has_system_spdlog",           "spdlog")
check_system_pkgconfig_package("has_system_gtest",            {"gtest", "gmock"})

-- hide options, hide targets, pack targets
includes("lua/hideoptions.lua")
includes("lua/hidetargets.lua")

-- some of the required libraries use our own repository
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
includes("lua/backends.lua")
includes("lua/pack.lua")

-- header-only libraries
if not has_config("has_std_out_ptr")  then add_requires("out_ptr") end
if not has_config("has_std_expected") then add_requires("zeus_expected") end
if not has_config("has_std_format")  then add_requires("fmt") end

-- normal libraries
add_requires(
    "spdlog",
    "ilias",
    "neko-proto-tools"
)

-- msvc flags
add_cxxflags("cl::/Zc:preprocessor")


if not has_config("3rd_custom") then
-- configurations of required libraries
add_requireconfs("**out_ptr",       {override = true, version = "x.x.x"})
add_requireconfs("**zeus_expected", {override = true, version = "x.x.x"})
-- normal libraries' dependencies configurations
add_requireconfs("**fmt",      {override = true, system = useSystemFmt, version = "x.x.x", configs = {shared = is_config("3rd_kind", "shared"), debug = is_config("3rd_mode", "debug"), header_only = false}})
add_requireconfs("**spdlog",    {override = true, system = useSystemSpdlog, version = "x.x.x", configs = {shared = is_config("3rd_kind", "shared"), debug = is_config("3rd_mode", "debug"), header_only = false, fmt_external = useSystemSpdlog or not has_config("has_std_format"), std_format = not useSystemSpdlog and has_config("has_std_format"), wchar = true, wchar_console = true}})
add_requireconfs("**ilias",     {override = true, version = "x.x.x", configs = {debug = is_config("3rd_mode", "debug"), stdcxx = stdcxx_version(), fmt = not has_config("has_std_format")}})
add_requireconfs("**neko-proto-tools", {override = true, version = "x.x.x", configs = {debug = is_config("3rd_mode", "debug"), enable_simdjson = false, enable_rapidjson = true, enable_communication = false, enable_fmt = false, enable_spdlog = false, enable_rapidxml = false, enable_jsonrpc = false, enable_protocol = false, enable_tomlplusplus = true}})
end

-- subdirectories
-- includes("src/*/xmake.lua")
-- includes("exec/*/xmake.lua")
includes("tests/xmake.lua")

-- The GUI stays opt-in so a normal command-line build never needs a Qt SDK.
if has_config("enable_gui") then
    add_requires("qt6quick 6.8.3")
    includes("ui/xmake.lua")
end

target("mksync")
    set_kind("binary")
    set_installdir(path.join(os.projectdir(), "build", "install"))
    add_packages("ilias")
    add_packages("spdlog")
    add_packages("neko-proto-tools")
    
    mks_add_backend_packages()

    if not has_config("has_std_format") then
        add_packages("fmt")
    end
    
    add_includedirs("src")
    add_files("src/*.cpp")
    add_files("src/app/**.cpp")
    add_files("src/config/**.cpp")
    add_files("src/core/**.cpp")
    add_files("src/rpc/**.cpp")
    mks_add_backend_sources()
    add_installfiles("LICENSE", "README.md", "README_zh.md", {prefixdir = "share/doc/mksync"})

    -- Pch
    set_pcxxheader("src/config/pch.hpp")
    -- add_forceincludes("src/config/pch.hpp", {sourcekinds = "cxx"})

    -- GCC's std::stacktrace implementation is provided by libstdc++exp.
    if is_plat("mingw", "linux") then
        add_links("stdc++exp")
    end

    if is_plat("linux") then
        set_policy("install.strip_packagelibs", true)
        add_rpathdirs("$ORIGIN/../lib", {installonly = true})
    end

    -- Reflection
    if stdcxx_version() == 26 and is_tool("cxx", "gcc") then
        add_cxxflags("-freflection", {force = true})
    end

    -- Config dir
    set_configdir("src/config/")
    set_configvar("MKS_SHARED_BUILD", is_kind("shared") and true or false)
    add_configfiles("src/config/*.in", {public = true})
target_end()
