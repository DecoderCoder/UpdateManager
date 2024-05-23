#pragma once
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include "D3DX11.h"

#include <dxgi.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")

class Window;

static bool FalseBool = false;

namespace DirectX {
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();
	bool LoadTextureFromFile(std::wstring filename, ID3D11ShaderResourceView*& out);
	void Init();
	void Deinit();
	void Render();
	void AddWindow(Window* window);
	LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	inline std::vector<Window*> Windows;
}

class Window {
	bool Showed = false;
protected:
	std::string windowName;
public:
	bool Opened = true;
	Window();

	Window(bool show);
	virtual void Show();
	virtual void Close();
	virtual bool Render();
};