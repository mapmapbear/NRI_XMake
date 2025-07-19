add_rules("mode.debug", "mode.release")
set_languages("c++20")

-- set_policy("build.warning", true)
-- set_warnings("all", "extra")

if is_mode("debug") then
    add_defines("DEBUG")
    set_symbols("debug")
    set_optimize("none")
end

add_requires("glfw", "glm", "assimp", "spdlog", "taskflow")
add_requires("meshoptimizer", {debug = true})

target("Detex")
    set_kind("static")
    add_includedirs("3rd/Detex/", {public = true})
    add_files("3rd/Detex/*.c")

target("D3D12Ma")
    set_kind("static")
    add_includedirs("3rd/d3d12ma/include", {public = true})
    add_files("3rd/d3d12ma/src/D3D12MemAlloc.cpp")

target("ImGUI")
    set_kind("static")
    add_includedirs("3rd/imgui/", {public = true})
    add_files("3rd/imgui/*.cpp")

target("TinyddsLoader")
    set_kind("static")
    add_includedirs("3rd/tinyddsLoader/", {public = true})
    add_files("3rd/tinyddsLoader/tinyddsLoader.cpp")

target("ShaderMake")
    set_kind("static")
    add_includedirs("3rd/ShaderMake/", {public = true})
    add_files("3rd/ShaderMake/*.cpp")
    add_files("3rd/ShaderMake/*.c")

target("NRI")
    set_kind("static")
    add_deps("D3D12Ma")
    add_deps("ShaderMake")
    add_defines("NOMINMAX", "NRI_ENABLE_D3D12_SUPPORT", "NRI_ENABLE_IMGUI_EXTENSION")
    if is_mode("debug") then
        add_defines("NRI_ENABLE_DEBUG_NAMES_AND_ANNOTATIONS", "NRI_ENABLE_AGILITY_SDK_SUPPORT", "NRI_ENABLE_VALIDATION_SUPPORT")
    end
    add_includedirs("3rd/", {public = true})
    add_includedirs("3rd/NRI/Include", {public = true})
    add_includedirs("3rd/NRI/Source/Shared/", {public = true})
    add_includedirs("3rd/d3d12ma/include/", {public = true})
    add_includedirs("3rd/WinPixEventRuntime/Include/WinPixEventRuntime/")
    add_includedirs("3rd/Directx12Agility/build/native/include", {public = true})
    add_includedirs("3rd/NRI/_Shaders/", {public = true})
    add_files("3rd/NRI/Source/Creation/*.cpp")
    add_files("3rd/NRI/Source/D3D12/*.cpp")
    add_files("3rd/NRI/Source/Shared/*.cpp")
    add_files("3rd/NRI/Source/NONE/*.cpp")
    add_files("3rd/NRI/Source/Validation/*.cpp")
    add_links("3rd/WinPixEventRuntime/bin/x64/WinPixEventRuntime.lib")
    add_syslinks("dxgi", "d3d12", "dxguid")
    -- on_config(function(target)
    --     local header_path = "3rd/NRI/Include/NRIAgilitySDK.h"
    --     local agility_sdk_version = "616"
    --     local agility_sdk_dir = "AgilitySDK"

    --     local content = [[
    --     // This file is auto-generated during project deployment. Do not modify!
    --     #ifdef __cplusplus
    --     extern "C" {
    --     #endif

    --     __declspec(dllexport) extern const uint32_t D3D12SDKVersion = ]] .. agility_sdk_version .. [[;
    --     __declspec(dllexport) extern const char* D3D12SDKPath = "]] .. agility_sdk_dir .. [[/x64/";

    --     #ifdef __cplusplus
    --     }
    --     #endif
    --     ]]

    --     io.writefile(header_path, content)

    --     local runtime_dir = get_config("rundir") or "bin"
    --     -- local agility_sdk_dir = get_config("NRI_AGILITY_SDK_DIR") or "AgilitySDK"
    --     local build_dir = get_config("buildir") or "build"
    --     local dest_path = path.join(path.join(build_dir, "windows/x64/debug"), "AgilitySDK/x64")
    --     if os.isdir(dest_path) then
    --         print("delete old dest dir...")
    --         os.rm(dest_path)
    --         print("NRI: copying Agility SDK binaries to '" .. dest_path .. "'")
    --         os.cp("3rd/Directx12Agility/build/native/bin/x64", dest_path)
    --     else 
    --         print("NRI: copying Agility SDK binaries to '" .. dest_path .. "'")
    --         os.cp("3rd/Directx12Agility/build/native/bin/x64", dest_path)
    --     end
        
    -- end)


target("NRIFramework")
    set_kind("static")
    add_deps("NRI", "ImGUI", "TinyddsLoader")
    add_defines("NRI_ENABLE_AGILITY_SDK_SUPPORT")
    add_includedirs("3rd/NRI_Framework/Include", {public = true})
    add_includedirs("3rd/NRI/Include", {public = true})
    add_includedirs("3rd/imgui/", {public = true})
    add_includedirs("3rd/Detex", {public = true})
    add_includedirs("3rd/tinyddsLoader/", {public = true})
    add_includedirs("3rd/nvtt3/include/", {public = true})
    add_includedirs("3rd/", {public = true})
    add_files("3rd/NRI_Framework/Source/*.cpp")
    add_links("3rd/nvtt3/lib/x64-v142/nvtt30205.lib")
    add_packages("glfw", "glm", "assimp", "spdlog")

target("DemoApp")
    set_kind("binary")
    add_defines("HDR_ENABLE", "RZ")
    add_deps("NRIFramework", "Detex", "NRI", "ImGUI")
    add_includedirs("3rd/NRI_Framework/Include", {public = true})
    add_includedirs("3rd/Detex", {public = true})
    add_includedirs("source/", {public = true})
    add_packages("glfw", "glm", "assimp", "spdlog", "meshoptimizer", "taskflow")
    add_files("main.cpp", "source/**.cpp")
    set_warnings("all")
    
target("ShaderCompiler")
    set_kind("phony") -- 这里可以是 phony，避免 xmake 生成实际的二进制文件
    set_default(false) -- 让它不在默认 `xmake build` 触发
    add_files("shaders/**.hlsl")

    on_build(function (target)
        import("lib.detect.find_tool")
        local dxc = assert(find_tool("dxc"), "dxc not found!")
        local shaderMakePath = path.join(os.scriptdir(), "3rd/ShaderMake/bin/Release/ShaderMake.exe")
        local shader_output_path = path.join(target:targetdir(), "_Shaders1")
        os.mkdir(shader_output_path)
        local args = {
            "--useAPI", "--binary", "--flatten", "--stripReflection", "--WX", "--PDB",
            "--sRegShift", "100", "--tRegShift", "200", "--bRegShift", "300", "--uRegShift", "400", "-X", " -Gis",
            "--sourceDir", path.join(os.scriptdir(), "Shaders"),
            "-c", path.join(os.scriptdir(), "Shaders.cfg"),
            "-o", shader_output_path,
            "-I", path.join(os.scriptdir(), "3rd/NRI/Include"),
            "-p", "DXIL", "--compiler", "D:/Source/GitProject/dxc_2025_02_20/bin/x64/dxc.exe"
        }
        os.execv(shaderMakePath, args)
    end)

