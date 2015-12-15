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
	startproject "expressions"
	includedirs { "../3rdparty" }
	
project "expressions"
	kind "ConsoleApp"
	
	files { "../src/expressions/main.cpp" }
	defaultConfigurations()
