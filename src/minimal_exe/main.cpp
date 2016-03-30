#include <windows.h>

#pragma comment(linker, "/ALIGN:16")// Merge sections

// Single entrypoint
int mainCRTStartup()
{
	return 0;
}