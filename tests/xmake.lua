option("enable_tests")
    set_default(true)
    set_showmenu(true)
    set_description("Enable test targets")
    set_category("enable test")
option_end()

if get_config("enable_tests") ~= false then
add_requires("gtest")
add_packages("gtest")
add_requireconfs("**gtest", {
    override = true,
    system = has_config("has_system_gtest"),
    version = "x.x.x"
})

local test_root = os.scriptdir()

local function test_group(file)
    local relative = path.relative(path.directory(file), test_root)
    local group = path.filename(relative)
    if group == nil or group == "" or group == "." then
        return "core"
    end
    return group
end

local function add_common_test_packages()
    add_packages("gtest", "neko-proto-tools", "fmt", "ilias", "spdlog")
    if not has_config("has_std_format") then
        add_packages("fmt")
    end
end

function mks_apply_test_settings(file)
    local group = test_group(file)
    set_kind("binary")
    set_default(false)
    set_group("tests/" .. group)
    add_includedirs(path.join(os.projectdir(), "src"))
    add_includedirs(test_root)
    if stdcxx_version() == 26 and is_tool("cxx", "gcc") then
        add_cxxflags("-freflection", {force = true})
    end
    add_common_test_packages()
    add_tests(stdcxx():gsub("%+", "p", 2), {group = group, kind = "binary", languages = stdcxx()})
end

function mks_add_default_test(file)
    target(path.basename(file))
        mks_apply_test_settings(file)
        add_files(file)
    target_end()
end

for _, file in ipairs(os.files(path.join(test_root, "**.cpp"))) do
    local dir = path.directory(file)
    local name = path.basename(file)
    local conf = path.join(dir, name .. ".lua")

    if name:sub(1, 5) == "test_" then
        if os.exists(conf) then
            includes(conf)
        else
            mks_add_default_test(file)
        end
    end
end
end
