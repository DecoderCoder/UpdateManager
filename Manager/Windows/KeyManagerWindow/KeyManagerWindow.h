#pragma once
#include "../../DirectX/DirectX.h"
#include "../../UpdateManager/UpdateManager.h"
#include <thread>

class KeyManagerWindow : public Window {
private:
	int selectedHost = -1;
	UpdateManager::Host::AccessGroup* selectedAccessGroup;
public:
	KeyManagerWindow();
	KeyManagerWindow(int selectedHost);
	virtual bool Render();
};