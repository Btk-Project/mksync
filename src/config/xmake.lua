target("config")
    set_kind("headeronly")

    add_includedirs("include", {public = true})
    add_headerfiles("include/(**)")

    set_configdir("include/mksync/")
    set_configvar("MKS_SHARED_BUILD", is_kind("shared") and true or false)
    -- if has_config("luanch") then
    --     set_configvar("TEST_LUANCH", get_config("luanch"))
    -- end
    add_configfiles("*.in", {public = true})
target_end()