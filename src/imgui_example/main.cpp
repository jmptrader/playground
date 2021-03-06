#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstring>
#include "imgui/imgui.h"
#include <vector>


HWND g_hWnd;
HDC g_hDC;
HGLRC g_hRC;
GLuint g_font_texture;
static const int MAX_PATH_LENGTH = 256;
typedef char Path[MAX_PATH_LENGTH];
typedef unsigned int u32;


const char* findSubstring(const char* haystack, const char* needle)
{
	return strstr(haystack, needle);
}


void copyString(char* dest, const char* src)
{
	strcpy(dest, src);
}


void catString(char* dest, const char* src)
{
	strcat(dest, src);
}


int compareString(const char* a, const char* b)
{
	return strcmp(a, b);
}


int compareIString(const char* a, const char* b)
{
	return _stricmp(a, b);
}

struct File
{
	enum Flags
	{
		SELECTED = 1,
		DIRECTORY = 2
	};

	Path name;
	size_t size;
	u32 flags;
};


struct Files
{
	std::vector<File> files;
	Path path;
	char filter[50];
} g_files;


enum class Columns
{
	NAME,
	SIZE
};


void sortBy(Columns column)
{
	if (column == Columns::NAME)
	{
		auto cmpFiles = [](const void* a, const void* b) -> int {
			auto* f0 = static_cast<const File*>(a);
			auto* f1 = static_cast<const File*>(b);
			bool is_f0_dir = (f0->flags & File::DIRECTORY) != 0;
			bool is_f1_dir = (f1->flags & File::DIRECTORY) != 0;
			if (f0->name[0] == '.') return -1;
			if (f1->name[0] == '.') return 1;
			if (is_f0_dir && !is_f1_dir) return -1;
			if (is_f1_dir && !is_f0_dir) return 1;
			auto x = -compareIString(f1->name, f0->name);
			return x;
		};

		qsort(&g_files.files[0], g_files.files.size(), sizeof(g_files.files[0]), cmpFiles);
	}
	else
	{
		auto cmpFiles = [](const void* a, const void* b) -> int {
			auto* f0 = static_cast<const File*>(a);
			auto* f1 = static_cast<const File*>(b);
			bool is_f0_dir = (f0->flags & File::DIRECTORY) != 0;
			bool is_f1_dir = (f1->flags & File::DIRECTORY) != 0;
			if (is_f0_dir && !is_f1_dir) return 1;
			if (is_f1_dir && !is_f0_dir) return -1;
			if (f0->name[0] == '.') return -1;
			if (f1->name[0] == '.') return 1;
			return int(f1->size - f0->size);
		};

		qsort(&g_files.files[0], g_files.files.size(), sizeof(g_files.files[0]), cmpFiles);
	}
}


void normalizePath(Path& dest, const char* src)
{
	copyString(dest, src);
}


void fillFileList(const char* path)
{
	g_files.filter[0] = 0;
	WIN32_FIND_DATA ffd = {};
	Path ffd_path;
	normalizePath(ffd_path, path);
	catString(ffd_path, "/*");
	HANDLE handle = FindFirstFile(ffd_path, &ffd);
	if (!handle) return;
	copyString(g_files.path, path);
	g_files.files.reserve(4096);
	g_files.files.clear();
	do
	{
		if (ffd.cFileName[0] == '.' && ffd.cFileName[1] != '.') continue;
		g_files.files.emplace_back();
		auto& tmp = g_files.files.back();
		tmp.flags = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? File::DIRECTORY : 0;
		tmp.size = ffd.nFileSizeLow;
		copyString(tmp.name, ffd.cFileName);
	} while (FindNextFile(handle, &ffd));

	if (g_files.files.empty()) return;
	sortBy(Columns::NAME);
}


void showFileList()
{
	if (g_files.files.empty()) return;

	if (ImGui::InputText("", g_files.path, sizeof(g_files.path), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		fillFileList(g_files.path);
	}

	ImGui::Columns(2);
	if (ImGui::Selectable("Name")) sortBy(Columns::NAME);
	ImGui::NextColumn();
	if (ImGui::Selectable("Size")) sortBy(Columns::SIZE);
	ImGui::NextColumn();
	ImGui::Separator();
	for (auto& file : g_files.files)
	{
		if (g_files.filter[0] != 0 && !findSubstring(file.name, g_files.filter)) continue;

		bool selected = file.flags & File::SELECTED;
		if (ImGui::Selectable(file.name, selected, ImGuiSelectableFlags_AllowDoubleClick) &&
			file.flags & File::DIRECTORY)
		{
			Path tmp;
			copyString(tmp, g_files.path);
			catString(tmp, "/");
			catString(tmp, file.name);
			fillFileList(tmp);
			ImGui::Columns();
			return;
		}
		ImGui::NextColumn();
		if (file.flags & File::DIRECTORY)
		{
			ImGui::Text("DIR");
		}
		else
		{
			ImGui::Text("%16lu B", file.size);
		}
		ImGui::NextColumn();
	}
	ImGui::Columns();
	ImGui::InputText("Filter", g_files.filter, sizeof(g_files.filter));
}



void onGUI()
{
	auto& io = ImGui::GetIO();
	io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	io.KeyCtrl = (GetKeyState(VK_MENU) & 0x8000) != 0;

	ImGui::NewFrame();
	showFileList();
	ImGui::EndFrame();
}


void onResize(int width, int height)
{
	auto& io = ImGui::GetIO();
	io.DisplaySize.x = (float)width;
	io.DisplaySize.y = (float)height;
}


LRESULT WINAPI WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_SYSCOMMAND:
		{
			bool is_alt_key_menu = wParam == SC_KEYMENU && (lParam >> 16) <= 0;
			if (is_alt_key_menu) return 0;
			break;
		}
		case WM_KEYUP:
		case WM_SYSKEYUP: ImGui::GetIO().KeysDown[(int)wParam] = false; break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN: ImGui::GetIO().KeysDown[(int)wParam] = true; break;
		case WM_CHAR: ImGui::GetIO().AddInputCharacter((int)wParam); break;
		case WM_RBUTTONDOWN: ImGui::GetIO().MouseDown[1] = true; break;
		case WM_RBUTTONUP: ImGui::GetIO().MouseDown[1] = false; break;
		case WM_LBUTTONDOWN: ImGui::GetIO().MouseDown[0] = true; break;
		case WM_LBUTTONUP: ImGui::GetIO().MouseDown[0] = false; break;
		case WM_MOUSEMOVE:
		{
			POINT p;
			p.x = ((int)(short)LOWORD(lParam));
			p.y = ((int)(short)HIWORD(lParam));
			ImGuiIO& io = ImGui::GetIO();
			io.MousePos.x = (float)p.x;
			io.MousePos.y = (float)p.y;
		}
		break;
		case WM_SIZE:
		{
			RECT rect;
			GetClientRect(hWnd, &rect);
			onResize(rect.right - rect.left, rect.bottom - rect.top); 
			break;
		}
		case WM_CLOSE: PostQuitMessage(0); return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

HWND CreateOpenGLWindow(char* title, int x, int y, int width, int height, BYTE type, DWORD flags)
{
	WNDCLASS wc;
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "ImGUI";

	if (!RegisterClass(&wc)) return NULL;

	HWND hWnd = CreateWindow("ImGUI",
		title,
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		x,
		y,
		width,
		height,
		NULL,
		NULL,
		wc.hInstance,
		NULL);

	if (hWnd == NULL) return NULL;

	g_hDC = GetDC(hWnd);
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | flags;
	pfd.iPixelType = type;
	pfd.cColorBits = 32;

	int pf = ChoosePixelFormat(g_hDC, &pfd);
	if (pf == 0) return NULL;
	if (SetPixelFormat(g_hDC, pf, &pfd) == FALSE) return NULL;

	DescribePixelFormat(g_hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
	ReleaseDC(hWnd, g_hDC);
	return hWnd;
}


void imGUICallback(ImDrawData* draw_data)
{
	ImGuiIO& io = ImGui::GetIO();
	int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
	int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
	if (fb_width == 0 || fb_height == 0)
		return;
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);

	GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnable(GL_TEXTURE_2D);

	glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	#define OFFSETOF(TYPE, ELEMENT) ((size_t) & (((TYPE*)0)->ELEMENT))
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const unsigned char* vtx_buffer = (const unsigned char*)&cmd_list->VtxBuffer.front();
		const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();
		glVertexPointer(
			2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, pos)));
		glTexCoordPointer(
			2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, uv)));
		glColorPointer(4,
			GL_UNSIGNED_BYTE,
			sizeof(ImDrawVert),
			(void*)(vtx_buffer + OFFSETOF(ImDrawVert, col)));

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				GLuint texture_id =
					pcmd->TextureId ? (GLuint)(intptr_t)pcmd->TextureId : g_font_texture;

				glBindTexture(GL_TEXTURE_2D, texture_id);
				glScissor((int)pcmd->ClipRect.x,
					(int)(fb_height - pcmd->ClipRect.w),
					(int)(pcmd->ClipRect.z - pcmd->ClipRect.x),
					(int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
				glDrawElements(GL_TRIANGLES,
					(GLsizei)pcmd->ElemCount,
					sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
					idx_buffer);
			}
			idx_buffer += pcmd->ElemCount;
		}
	}
	#undef OFFSETOF

	// Restore modified state
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glBindTexture(GL_TEXTURE_2D, last_texture);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
}


void initImGUI()
{
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &g_font_texture);
	glBindTexture(GL_TEXTURE_2D, g_font_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, pixels);

	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';
	io.RenderDrawListsFn = imGUICallback;
}


bool init()
{
	g_hWnd = CreateOpenGLWindow("minimal", 0, 0, 800, 600, PFD_TYPE_RGBA, 0);
	if (g_hWnd == NULL) return false;

	g_hDC = GetDC(g_hWnd);
	g_hRC = wglCreateContext(g_hDC);
	wglMakeCurrent(g_hDC, g_hRC);

	ShowWindow(g_hWnd, SW_SHOW);
	initImGUI();

	Path tmp;
	GetCurrentDirectory(sizeof(tmp), tmp);
	fillFileList(tmp);
	return true;
}


INT WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	init();

	bool finished = false;
	while (!finished)
	{
		MSG msg;
		while (PeekMessage(&msg, g_hWnd, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		onGUI();
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();
		SwapBuffers(g_hDC);
	}

	wglMakeCurrent(NULL, NULL);
	ReleaseDC(g_hWnd, g_hDC);
	wglDeleteContext(g_hRC);
	DestroyWindow(g_hWnd);

	return 0;
}