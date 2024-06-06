#pragma once
#include "../../DirectX/DirectX.h"
#include "../../UpdateManager/UpdateManager.h"
#include <thread>
#include "../../Global.h"
#include "../../Settings.h"

class AboutWindow : public Window {
public:
	virtual bool Render();
};