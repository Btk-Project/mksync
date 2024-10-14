

target("Base")
    set_kind("headeronly")

    -- Pre-Define
    if is_kind("shared") then
        add_defines("MKS_DLL", {public = true})
    end

    -- target files
    add_includedirs("include", {public = true})
    add_headerfiles("include/(**)")
target_end()