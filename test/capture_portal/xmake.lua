

if is_host("linux") and not is_plat("corss") then
target("capture_portal")
    set_kind("binary")
    set_default(false)
    set_targetdir("$(testdir)")

    add_deps("config", "base")
    add_packages("libportal", "out_ptr", "sobjectizer", "spdlog")

    -- add_includedirs("include", {public = true})
    -- add_headerfiles("include/(**)")
    -- add_headerfiles("src/**.hpp", "src/**.h", {install = false})
    add_files("src/**.cpp")

    after_build(function (target) 
        import("lua.auto", {rootdir = os.projectdir()})
        auto().target_autoclean(target)
        -- auto().binary_autoluanch(target)
    end)
target_end()
end