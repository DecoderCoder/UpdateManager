#include "Manager.h"

#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

MainWindow* mainWindow;

void Main() {
	//UpdateManager::GetExecutableFolder();
}

void RenderThread() {
	DirectX::Init();
	while (DirectX::Windows.size() > 0) {
		DirectX::Render();
	}
}

int main()
{
	mainWindow = new MainWindow();
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)RenderThread, NULL, NULL, NULL);

	while (!GetAsyncKeyState(VK_END)) {
		Sleep(1);
		if (DirectX::Windows.size() == 0)
			return 0;
	}
	DirectX::Deinit();
}