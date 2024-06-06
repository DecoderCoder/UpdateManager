#include "KeyManagerWindow.h"
#include "../../Utils/uuid_v4/uuid_v4.h"

char inputAccessGroupNameBuffer[256] = { 0 };
char inputAccessGroupBuffer[37] = { 0 };

char inputKeyNameBuffer[37] = { 0 };
char inputKeyBuffer[33] = { 0 };

UUIDv4::UUIDGenerator<std::mt19937_64> uuidGenerator;

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
	ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
	ImGui::Begin("Key Manager", &this->Opened, ImGuiWindowFlags_MenuBar);
	bool disabled = false;
	if (!Window::Render()) {
		ImGui::End();
		return false;
	}

	ImGui::Columns(3);

	ImGui::Text("Host");
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##hosts", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::CalcTextSize("abc").y - ImGui::GetStyle().FramePadding.y * 4))) {
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
	if (ImGui::BeginListBox("##accessgroups", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::CalcTextSize("abc").y - ImGui::GetStyle().FramePadding.y * 4))) {
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
	if (ImGui::BeginListBox("##keys", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::CalcTextSize("abc").y - ImGui::GetStyle().FramePadding.y * 4))) {
		if (this->selectedAccessGroup != nullptr) {
			auto ag = &selectedAccessGroup->Keys;
			for (int i = 0; i < ag->size(); i++) {
				ImGui::Selectable(ag->at(i).Name.c_str());
			}
		}
		ImGui::EndListBox();
	}
	if (ImGui::Button("+##key", ImVec2(30, 0))) {
		ImGui::OpenPopup("Add Key");
	}
	if (ImGui::BeginPopupModal("Add Key", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text(" Name ");
		ImGui::SetNextItemWidth(ImGui::CalcTextSize("53836da6-de2f-44b8-8454-1f0ccb4b1e65").x + ImGui::GetStyle().FramePadding.x * 4);
		ImGui::InputText("##keyname", inputKeyNameBuffer, sizeof(inputKeyNameBuffer));
		ImGui::SameLine();
		if (ImGui::Button("Generate UUID##key")) {
			string uuid;
			uuidGenerator.getUUID().str(uuid);
			uuid.copy(inputKeyNameBuffer, sizeof(inputKeyNameBuffer), 0);
		}
		ImGui::Text(" Key");
		ImGui::SameLine();
		ImGui::TextDisabled("(12345678-1234-1234-1234-0123456789ab)");
		ImGui::SetNextItemWidth(ImGui::CalcTextSize("53836da6-de2f-44b8-8454-1f0ccb4b1e65").x + ImGui::GetStyle().FramePadding.x * 4);
		ImGui::InputText("##keyvalue", inputKeyBuffer, sizeof(inputKeyBuffer), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
		ImGui::SameLine();
		if (ImGui::Button("Random##key", ImVec2(-1, 0))) {
			string hex = "ABCDEF0123456789";
			string s;
			for (int i = 0; i < 32; i++) {
				s.push_back(hex[rand() % hex.size()]);
			}
			s.copy(inputKeyBuffer, sizeof(inputKeyBuffer), 0);
		}

		ImGui::Spacing();

		disabled = !Utils::IsUUID(inputKeyNameBuffer);

		if (!disabled)
			disabled = strlen(inputKeyBuffer) != 32;
		if (disabled)
			ImGui::BeginDisabled();
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().FramePadding.x, 0))) {
			if (selectedAccessGroup) {
				
				auto bytes = Utils::hex_to_bytes(string(inputKeyBuffer));
				this->selectedAccessGroup->AddKey(string(inputKeyNameBuffer), base64_encode(bytes.data(), bytes.size()), UpdateManager::GetHosts()->at(selectedHost).IsAdmin);
				ImGui::CloseCurrentPopup();
			}
		}
		if (disabled)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Add Access Group", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		string inputAccessGroupName = string(inputAccessGroupNameBuffer);
		string inputAccessGroup = string(inputAccessGroupBuffer);

		ImGui::Text(" Name");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputText("##accessgroupname", inputAccessGroupNameBuffer, sizeof(inputAccessGroupNameBuffer));
		ImGui::Text(" Access Group ");
		ImGui::SameLine();
		ImGui::TextDisabled("(12345678-1234-1234-1234-0123456789ab)");
		ImGui::SetNextItemWidth(ImGui::CalcTextSize("53836da6-de2f-44b8-8454-1f0ccb4b1e65").x + ImGui::GetStyle().FramePadding.x * 4);
		ImGui::InputText("##accessgroup", inputAccessGroupBuffer, sizeof(inputAccessGroupBuffer));
		ImGui::SameLine();
		if (ImGui::Button("Generate UUID##accessgroup")) {
			string uuid;
			uuidGenerator.getUUID().str(uuid);
			uuid.copy(inputAccessGroupBuffer, sizeof(inputAccessGroupBuffer), 0);
		}
		ImGui::Spacing();
		disabled = !Utils::IsUUID(inputAccessGroup);

		if (!disabled)
			disabled = inputAccessGroupName == "";
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
