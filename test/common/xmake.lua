add_requires("gtest")

option("memcheck")
    set_default(false)
    set_showmenu(true)

-- Make all files in the directory into targets
for _, file in ipairs(os.files("./**/test_*.cpp")) do
    local name = path.basename(file)
    local dir = path.directory(file)
    local conf_path = dir .. "/" .. name .. ".lua"

    -- If this file require a specific configuration, load it, and skip the auto target creation
    if os.exists(conf_path) then 
        includes(conf_path)
        goto continue
    end

    -- Otherwise, create a target for this file, in most case, it should enough
    target(name)
        set_kind("binary")
        set_default(false)
        set_encodings("utf-8")
        add_defines("NEKO_PROTO_STATIC")
        add_defines("ILIAS_ENABLE_LOG", "ILIAS_USE_FMT")
        add_includedirs("../../src/base/include/")
        add_deps("base")
        add_packages("gtest", "ilias", "fmt", "neko-proto", "spdlog", "cxxopts", "rapidjson")
        set_targetdir("$(testdir)")
        set_languages("c++20")
        add_files(file)
        -- set test in different cpp versions
        local cpp_versions = {"c++20"}
        for i = 1, #cpp_versions do
            add_tests(string.gsub(cpp_versions[i], '+', 'p', 2), {group = "proto", kind = "binary", files = {file}, languages = cpp_versions[i], run_timeout = 1000})
        end
        on_run(function (target)
            local argv = {}
            if has_config("memcheck") then
                table.insert(argv, "--leak-check=full")
                table.insert(argv, target:targetfile())
                os.execv("valgrind", argv)
            else
                os.run(target:targetfile())
            end
        end)
    target_end()

    ::continue::
end