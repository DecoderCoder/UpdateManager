#pragma once
#include "../../DirectX/DirectX.h"
#include "../../UpdateManager/UpdateManager.h"
#include <thread>

class KeyManagerWindow : public Window {
private:
	int selectedHost = -1;
	string selectedAccessGroup = "";
public:
	KeyManagerWindow();
	KeyManagerWindow(int selectedHost);
	virtual bool Render();
};