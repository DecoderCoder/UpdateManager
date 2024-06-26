#pragma once
#include "../../DirectX/DirectX.h"
#include "../../UpdateManager/UpdateManager.h"
#include <thread>
#include "../../Utils/Utils.h"

class KeyManagerWindow : public Window {
private:
	int selectedHost = -1;
	UpdateManager::AccessGroup* selectedAccessGroup;
public:
	KeyManagerWindow();
	KeyManagerWindow(int selectedHost);
	virtual bool Render();
};