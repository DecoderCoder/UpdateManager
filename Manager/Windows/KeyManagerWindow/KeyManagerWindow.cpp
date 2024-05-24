#include "KeyManagerWindow.h"

KeyManagerWindow::KeyManagerWindow() : Window()
{

}

KeyManagerWindow::KeyManagerWindow(int selectedHost) : Window()
{
	this->selectedHost = selectedHost;
}

bool KeyManagerWindow::Render()
{
	std::vector<UpdateManager::Host>* hosts = UpdateManager::GetHosts();
	ImGui::Begin("Key Manager", &this->Opened, ImGuiWindowFlags_MenuBar);

	if (!Window::Render()) {
		ImGui::End();
		return false;
	}

	ImGui::Columns(3);

	ImGui::Text("Host");
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##hosts")) {
		for (int i = 0; i < hosts->size(); i++) {
			if (ImGui::Selectable(hosts->at(i).Uri.c_str(), this->selectedHost == i)) {
				this->selectedHost = i;
				this->selectedAccessGroup = "";
			}
		}
		ImGui::EndListBox();
	}
	ImGui::NextColumn();
	ImGui::Text("Access Groups");
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##accessgroups")) {
		if (selectedHost > -1)
			for (auto obj : hosts->at(selectedHost).accessGroup) {
				if (ImGui::Selectable(obj.first.c_str(), obj.first == this->selectedAccessGroup)) {
					this->selectedAccessGroup = obj.first;
				}
			}
		ImGui::EndListBox();
	}
	ImGui::NextColumn();
	ImGui::Text("Keys");
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##keys")) {
		if (this->selectedAccessGroup != "") {
			auto ag = &hosts->at(this->selectedHost).accessGroup[this->selectedAccessGroup];
			for (int i = 0; i < ag->size(); i++) {
				ImGui::Selectable(ag->at(i).Name.c_str());
			}
		}
		ImGui::EndListBox();
	}
	ImGui::End();
	return true;
}
