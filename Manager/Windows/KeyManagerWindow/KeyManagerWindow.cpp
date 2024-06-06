#include "KeyManagerWindow.h"

char inputAccessGroupNameBuffer[256] = { 0 };
char inputAccessGroupBuffer[37] = { 0 };

void ResetAddKey() {
	memset(inputAccessGroupBuffer, 0, sizeof(inputAccessGroupBuffer));
}

KeyManagerWindow::KeyManagerWindow() : Window()
{

}

KeyManagerWindow::KeyManagerWindow(int selectedHost) : Window()
{
	this->selectedHost = selectedHost;
}

bool InputTextCentered(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL) {
	const float input_width = ImGui::CalcItemWidth();
	ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x - input_width) / 2.f);
	ImGui::Text("%s", label);
	ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x - input_width) / 2.f);
	return ImGui::InputText("##dummylabel", buf, buf_size, flags, callback, user_data);
}

bool KeyManagerWindow::Render()
{

	std::vector<UpdateManager::Host>* hosts = UpdateManager::GetHosts();
	ImGui::Begin("Key Manager", &this->Opened, ImGuiWindowFlags_MenuBar);
	bool disabled = false;
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
				this->selectedAccessGroup = nullptr;
			}
		}
		ImGui::EndListBox();
	}
	ImGui::NextColumn();
	ImGui::Text("Access Groups");
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##accessgroups")) {
		if (selectedHost > -1)
			for (auto& obj : hosts->at(selectedHost).accessGroups) {
				if (ImGui::Selectable(obj->Name.c_str(), obj == this->selectedAccessGroup)) {
					this->selectedAccessGroup = obj;
				}
			}
		ImGui::EndListBox();
	}
	disabled = selectedHost == -1 || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
	if (disabled)
		ImGui::BeginDisabled();
	if (ImGui::Button("+##accessgroup", ImVec2(30, 0))) {
		ImGui::OpenPopup("Add Access Group");
	}
	if (disabled)
		ImGui::EndDisabled();


	ImGui::NextColumn();
	ImGui::Text("Keys");
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##keys")) {
		if (this->selectedAccessGroup != nullptr) {
			auto ag = &selectedAccessGroup->keys;
			for (int i = 0; i < ag->size(); i++) {
				ImGui::Selectable(ag->at(i).Name.c_str());
			}
		}
		ImGui::EndListBox();
	}
	if (ImGui::Button("+##key", ImVec2(30, 0))) {
		ImGui::OpenPopup("Add Key");
	}

	if (ImGui::BeginPopupModal("Add Access Group", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		string inputAccessGroupName = string(inputAccessGroupNameBuffer);
		string inputAccessGroup = string(inputAccessGroupBuffer);

		ImGui::Text(" Name");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputText("##accessgroupname", inputAccessGroupNameBuffer, sizeof(inputAccessGroupNameBuffer));
		ImGui::Text(" Access Group");
		ImGui::SetNextItemWidth(ImGui::CalcTextSize("53836da6-de2f-44b8-8454-1f0ccb4b1e65").x + ImGui::GetStyle().FramePadding.x * 4);
		ImGui::InputText("##accessgroup", inputAccessGroupBuffer, sizeof(inputAccessGroupBuffer));
		ImGui::SameLine();
		if (ImGui::Button("Generate UUID")) {

		}

		for (int i = 0; i < inputAccessGroup.size(); i++) {
			if (i == 8 || i == 13 || i == 18 || i == 23)
				continue;
			if (!(inputAccessGroup[i] >= 48 && inputAccessGroup[i] <= 57 || inputAccessGroup[i] >= 97 && inputAccessGroup[i] <= 102))
			{
				disabled = true;
				break;
			}
		}
		if (inputAccessGroup.size() != 36)
			disabled = true;
		if (!disabled)
			disabled = !(inputAccessGroup[8] == '-' && inputAccessGroup[13] == '-' && inputAccessGroup[18] == '-' && inputAccessGroup[23] == '-');
		if (disabled)
			ImGui::BeginDisabled();
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().FramePadding.x, 0))) {
			auto host = &UpdateManager::GetHosts()->at(selectedHost);
			UpdateManager::Host::AddAccessGroupResponse resp;
			host->AddAccessGroup(inputAccessGroupName, inputAccessGroup, host->IsAdmin, &resp);
			if (resp == UpdateManager::Host::AddAccessGroupResponse::Success) {
				ImGui::CloseCurrentPopup();
			}
			else {
				ImGui::OpenPopup("Already exists##accessgroup");
			}
		}
		if (disabled)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::BeginPopupModal("Already exists##accessgroup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Access group with this name or value already exists");
			if (ImGui::Button("Close", ImVec2(-1, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::End();
	return true;
}
