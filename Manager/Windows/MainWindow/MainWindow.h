#pragma once
#include "../../Settings.h"
#include "../KeyManagerWindow/KeyManagerWindow.h"
#include "../ViewWindow/ViewWindow.h"
#include "../AboutWindow/AboutWindow.h"
#include "../../DirectX/DirectX.h"
#include "../../UpdateManager/UpdateManager.h"
#include <thread>

class MainWindow : public Window {
public:
	virtual bool Render();
};