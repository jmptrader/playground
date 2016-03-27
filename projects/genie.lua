local BINARY_DIR = "tmp/bin/"

function defaultConfigurations()
	configuration "Debug"
		targetdir(BINARY_DIR .. "Debug")
		defines { "DEBUG" }
		flags { "Symbols", "WinMain" }

	configuration "Release"
		targetdir(BINARY_DIR .. "Release")
		defines { "NDEBUG" }
		flags { "Optimize", "WinMain" }
end


solution "Playground"
	configurations { "Debug", "Release" }
	platforms { "x32", "x64" }
	flags { "FatalWarnings", "NoPCH" }
	location "tmp"
	language "C++"
	startproject "imgui_example"
	includedirs { "../3rdparty" }

project "expressions"
	kind "ConsoleApp"

	files { "../src/expressions/main.cpp", "genie.lua" }
	defaultConfigurations()

project "imgui_example"
	kind "WindowedApp"

	links { "opengl32" }
	files { "../3rdparty/imgui/*.cpp", "../3rdparty/imgui/*.h", "../src/imgui_example/main.cpp", "genie.lua" }
	defaultConfigurations()

project "link_test_lua"
	kind "StaticLib"

	files { "../3rdparty/lua/*.c", "../3rdparty/lua/*.h", "../3rdparty/lua/*.def", "genie.lua"  }
	defaultConfigurations()

project "link_test_dll"
	kind "SharedLib"

	linkoptions {"/DEF:\"../../3rdparty/lua/lua.def\""}
	files { "../src/link_test/dll.cpp", "genie.lua" }
	links { "link_test_lua" }
	defaultConfigurations()

project "link_test_main"
	kind "ConsoleApp"

	defines {"LUA_BUILD_AS_DLL"}
	files { "../src/link_test/main.cpp", "genie.lua" }
	links { "link_test_dll" }
	defaultConfigurations()
