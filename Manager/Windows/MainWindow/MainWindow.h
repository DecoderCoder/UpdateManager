#pragma once
#include "../KeyManagerWindow/KeyManagerWindow.h"
#include "../../DirectX/DirectX.h"
#include "../../UpdateManager/UpdateManager.h"
#include "../ViewWindow/ViewWindow.h"
#include <thread>

class MainWindow : public Window {
public:
	virtual bool Render();
};