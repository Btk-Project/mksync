add_requires("gtest")
add_packages("gtest")
add_requireconfs("**gtest", {
    override = true,
    system = has_config("has_system_gtest"),
    version = "x.x.x"
})

add_includedirs("../src")
if get_config("stdcxx") == 26 then
    add_cxxflags("-freflection", {force = true})
end

target("test_refl")
    set_kind("binary")
    set_default(false)
    add_files("refl.cpp")
    add_packages("gtest", "neko-proto-tools", "fmt", "ilias", "spdlog")
    if not has_config("has_std_format") then
        add_packages("fmt")
    end

    add_tests("test_refl")
target_end()

target("test_rpc_transport")
    set_kind("binary")
    set_default(false)
    add_files("rpc_transport.cpp")
    add_files("../src/rpc/message.cpp", "../src/rpc/transport.cpp")
    add_packages("gtest", "neko-proto-tools", "fmt", "ilias", "spdlog")
    if not has_config("has_std_format") then
        add_packages("fmt")
    end

    add_tests("test_rpc_transport")
target_end()
