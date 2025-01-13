add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("llvm-prebuilt","nlohmann_json")
add_requires("preloader v1.12.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end 

target("Troll")
    set_kind("binary")
    set_languages("c++20")
    set_symbols("debug")
    set_exceptions("none")
    add_headerfiles("src/(**.h)")
    add_includedirs("./src")
    add_defines("UNICODE")
    add_cxflags("/utf-8", "/EHa")
    add_files("src/**.cpp")
    add_packages("llvm-prebuilt", "nlohmann_json", "preloader")
    add_ldflags("/DELAYLOAD:Preloader.dll", "delayimp.lib")
