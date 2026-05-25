add_requires("gtest")
add_packages("gtest")

add_includedirs("../src")
add_cxxflags("-freflection", {force = true})

target("test_refl")
    set_kind("binary")
    add_files("refl.cpp")
    add_packages("gtest")
target_end()