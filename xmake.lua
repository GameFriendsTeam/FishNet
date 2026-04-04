set_project("fishnet")
set_version("2.0.0")
set_languages("c++20")

option("bedrock")
    set_default(false)
    set_showmenu(true)
    set_description("Include Minecraft Bedrock extension (src-bedrock/)")
option_end()

option("examples")
    set_default(false)
    set_showmenu(true)
    set_description("Build example programs")
option_end()

rule("fishnet.platform")
    on_load(function (target)
        if target:is_plat("windows") then
            target:add("cxxflags", "/W4", "/utf-8", {tools = {"cl", "clang_cl"}})
            target:add("cxxflags", "/wd4251", {tools = {"cl", "clang_cl"}})
            target:add("cxxflags", "/wd4275", {tools = {"cl", "clang_cl"}})
        else
            target:add("cxxflags", "-Wall", "-Wextra", "-Wpedantic")
            target:add("cxxflags", "-Wno-unused-parameter")
        end

        if is_mode("debug") then
            target:add("defines", "FISHNET_DEBUG")
            if target:is_plat("windows") then
                target:add("cxxflags", "/Zi", "/Od", "/FS", {tools = {"cl", "clang_cl"}})
                target:add("ldflags", "/DEBUG", {tools = {"link"}})
            else
                target:add("cxxflags", "-g", "-O0")
            end
        elseif is_mode("release") then
            if target:is_plat("windows") then
                target:add("cxxflags", "/O2", "/DNDEBUG", {tools = {"cl", "clang_cl"}})
                target:add("ldflags", "/OPT:REF", "/OPT:ICF", {tools = {"link"}})
            else
                target:add("cxxflags", "-O2", "-DNDEBUG")
                target:add("cxxflags", "-ffunction-sections", "-fdata-sections")
                target:add("ldflags", "-Wl,--gc-sections", {tools = {"gcc", "gxx", "clang", "clangxx"}})
            end
        end
    end)
rule_end()

rule("fishnet.shared")
    on_load(function (target)
        target:add("defines", "FISHNET_EXPORTS")

        if not target:is_plat("windows") then
            target:add("cxxflags", "-fvisibility=hidden")
            target:add("cxxflags", "-fvisibility-inlines-hidden")
        end
        if target:is_plat("linux") then
            local ver = target:version()
            if ver and ver.major then
                local ok, major = pcall(function() return ver:major() end)
                if ok and major then
                    target:add("ldflags", "-Wl,-soname,lib" .. target:name() .. ".so." .. major,
                               {tools = {"gcc", "gxx", "clang", "clangxx"}})
                end
            end
        end

        if target:is_plat("macosx") then
            target:add("ldflags", "-Wl,-install_name,@rpath/lib" .. target:name() .. ".dylib",
                       {tools = {"gcc", "gxx", "clang", "clangxx"}})
        end

        if target:is_plat("windows") and is_mode("debug") then
            target:add("ldflags", "/PDB:" .. target:name() .. ".pdb", {tools = {"link"}})
        end
    end)
rule_end()

rule("fishnet.link")
    on_load(function (target)
        if target:is_plat("windows") then
            target:add("syslinks", "ws2_32", "iphlpapi")
        elseif target:is_plat("linux") then
            target:add("syslinks", "pthread")
        elseif target:is_plat("macosx") then
        end
    end)
rule_end()

rule("fishnet.example")
    on_load(function (target)
        if target:is_plat("linux") then
            target:add("ldflags", "-Wl,-rpath,$ORIGIN", {tools = {"gcc", "gxx", "clang", "clangxx"}})
        elseif target:is_plat("macosx") then
            target:add("ldflags", "-Wl,-rpath,@executable_path", {tools = {"gcc", "gxx", "clang", "clangxx"}})
        end
    end)
rule_end()

target("fishnet")
    set_kind("shared")

    add_rules("fishnet.platform", "fishnet.shared", "fishnet.link")

    add_files("src/core/**.cpp")
    add_files("src/server/**.cpp")
    add_files("src/client/**.cpp")

    add_includedirs("src/include", {public = true})

    if has_config("bedrock") then
        add_files("src-bedrock/core/**.cpp")
        add_includedirs("src-bedrock/include", {public = true})
        add_defines("FISHNET_BEDROCK", {public = true})
    end

    set_targetdir("bin/$(mode)/$(plat)")
    set_objectdir("build/$(mode)/$(plat)/obj/fishnet")

if has_config("examples") then

    target("example_server")
        set_kind("binary")
        add_rules("fishnet.platform", "fishnet.example")
        add_files("examples/server.cpp")
        add_deps("fishnet")
        set_targetdir("bin/$(mode)/$(plat)")
        set_objectdir("build/$(mode)/$(plat)/obj/example_server")

    target("example_client")
        set_kind("binary")
        add_rules("fishnet.platform", "fishnet.example")
        add_files("examples/client.cpp")
        add_deps("fishnet")
        set_targetdir("bin/$(mode)/$(plat)")
        set_objectdir("build/$(mode)/$(plat)/obj/example_client")

    if has_config("bedrock") then

        target("example_bedrock_server")
            set_kind("binary")
            add_rules("fishnet.platform", "fishnet.example")
            add_files("examples/bedrock_server.cpp")
            add_deps("fishnet")
            set_targetdir("bin/$(mode)/$(plat)")
            set_objectdir("build/$(mode)/$(plat)/obj/example_bedrock_server")

        target("example_bedrock_client")
            set_kind("binary")
            add_rules("fishnet.platform", "fishnet.example")
            add_files("examples/bedrock_client.cpp")
            add_deps("fishnet")
            set_targetdir("bin/$(mode)/$(plat)")
            set_objectdir("build/$(mode)/$(plat)/obj/example_bedrock_client")

    end
end
