#include "../DirectX/DirectX.h"

Window::Window()
{
	DirectX::AddWindow(this);
}

Window::Window(bool show) : Window()
{
	this->Show();
}

void Window::Show()
{
	this->Showed = true;
}

void Window::Close()
{
	DirectX::Windows.erase(std::find(DirectX::Windows.begin(), DirectX::Windows.end(), this));
	delete(this);
}

bool Window::Render()
{
	if (this->Showed || ImGui::IsWindowAppearing()) {
		ImGui::SetWindowFocus();
		if (this->windowName != "") {
			auto result = ::FindWindowA(nullptr, (LPCSTR)this->windowName.c_str());
			BringWindowToTop(result);
		}
		this->Showed = false;
	}
	if (!this->Opened) {
		this->Close();
		return false;
	}
	return true;
}
