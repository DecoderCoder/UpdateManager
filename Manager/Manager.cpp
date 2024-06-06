#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include "UpdateManager/Utils/httplib/httplib.h" // if I want to use it, I use it
#include "Manager.h"
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

MainWindow* mainWindow;

void Main() {
	//UpdateManager::GetExecutableFolder();
}

//std::future<void> downloadAbout;

void RenderThread() {
	Settings::LoadSettings();
	{ // To minimize exe size
		httplib::Client cli("http://decodercoder.xyz");
		auto res = cli.Get("/about/my_avatar.png");
		Global::myAvatar.second = res->body.size();
		Global::myAvatar.first = (unsigned char*)malloc(Global::myAvatar.second);
		memcpy(Global::myAvatar.first, res->body.data(), Global::myAvatar.second);

		res = cli.Get("/about/ddma.png");
		Global::ddma.second = res->body.size();
		Global::ddma.first = (unsigned char*)malloc(Global::ddma.second);
		memcpy(Global::ddma.first, res->body.data(), Global::ddma.second);

		res = cli.Get("/about/fontRegular.ttf");
		Global::fontRegular.second = res->body.size();
		Global::fontRegular.first = (unsigned char*)malloc(Global::fontRegular.second);
		memcpy(Global::fontRegular.first, res->body.data(), Global::fontRegular.second);

		res = cli.Get("/about/fontMedium.ttf");
		Global::fontMedium.second = res->body.size();
		Global::fontMedium.first = (unsigned char*)malloc(Global::fontMedium.second);
		memcpy(Global::fontMedium.first, res->body.data(), Global::fontMedium.second);
	}

	DirectX::Init();
	ImGui::GetIO().Fonts->AddFontDefault();
	Global::fontRegular16 = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(Global::fontRegular.first, Global::fontRegular.second, 19);
	Global::fontMedium32 = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(Global::fontMedium.first, Global::fontMedium.second, 39);
	ImGui::GetIO().Fonts->Build();
	DirectX::LoadTextureFromMemory(Global::myAvatar.first, Global::myAvatar.second, Global::myAvatarImage);
	DirectX::LoadTextureFromMemory(Global::ddma.first, Global::ddma.second, Global::ddmaImage);
	//downloadAbout = std::async(std::launch::async, [&]() {

	//	});

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