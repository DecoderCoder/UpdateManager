#include "MainWindow.h"

char searchBuffer[256] = { 0 };

char inputHostNameBuffer[256] = { 0 };
char inputAdminLogin[256] = { 0 };
char inputAdminPassword[256] = { 0 };
char inputAppName[256] = { 0 };
string comboBoxAppAccessGroup;

int selectedHost = -1;
int selectedApp = -1;
int selectedBuild = -1;
std::vector<int> selectedFile;

int DownloadingCount;
std::map<UpdateManager::BuildFile*, std::pair<int, int>> UnpackingProgresses; // Build > (Progress, MaxProgress)
std::vector<UpdateManager::BuildFile*> openingBuilds;
ImGuiID dockId;

KeyManagerWindow* keyManagerWindow;

bool modalHostAdditional = false;
bool modalHostIsAdmin = false;

void ResetAddHost() {
	modalHostIsAdmin = false;
	memset(inputHostNameBuffer, 0, sizeof(inputHostNameBuffer));
	memset(inputAdminLogin, 0, sizeof(inputAdminLogin));
	memset(inputAdminPassword, 0, sizeof(inputAdminPassword));
}

void ResetAddApp() {
	memset(inputAppName, 0, sizeof(inputAppName));
	comboBoxAppAccessGroup = "";
}

const int dotsDelay = 300;

std::pair<char, int> appsDots = make_pair(0, 0);
std::pair<char, int> buildsDots = make_pair(0, 0);

string GetAppsText() {
	if (selectedHost != -1 && UpdateManager::GetHosts()->at(selectedHost).WaitingGetApps) {
		if (GetTickCount() > appsDots.second) { // appsLastDot
			if (appsDots.first == 3) // appsDotsCount
				appsDots.first = 1; // appsDotsCount
			else
				appsDots.first++; // appsDotsCount
			appsDots.second = GetTickCount() + dotsDelay; // appsLastDot
		}
	}
	else {
		appsDots.first = 0; // appsDotsCount
		appsDots.second = 0; // appsLastDot
	}
	string text = "Apps";
	for (int i = 0; i < appsDots.first; i++) // appsDotsCount
		text.append(".");
	return text;
}

string GetBuildsText() {
	if (selectedApp != -1 && UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).WaitingGetBuilds) {
		if (GetTickCount() > buildsDots.second) { // buildsLastDot
			if (buildsDots.first == 3) // buildsDotsCount
				buildsDots.first = 1; // buildsDotsCount
			else
				buildsDots.first++; // buildsDotsCount
			buildsDots.second = GetTickCount() + dotsDelay; // buildsLastDot
		}
	}
	else {
		buildsDots.first = 0; // buildsDotsCount
		buildsDots.second = 0; // buildsLastDot
	}
	string text = "Builds";
	for (int i = 0; i < buildsDots.first; i++) // buildsDotsCount
		text.append(".");
	return text;
}

bool MainWindow::Render()
{
	auto style = ImGui::GetStyle();
	style.WindowMinSize = ImVec2(900, 600);
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.FramePadding.y = 10;

	bool disabled = false;

	ImGui::Begin(_PROJECTNAME, &this->Opened, ImGuiWindowFlags_MenuBar);
	dockId = ImGui::GetID("viewwindows_dock");
	for (auto obj : UnpackingProgresses) {
		ImGui::Text(("Openning " + obj.first->Name).c_str());
		ImGui::ProgressBar((float)obj.second.first / (float)obj.second.second);
	}
	ImGui::SetNextItemWidth(-1);


	if (DownloadingCount > 0) {
		ImGui::SetNextItemWidth(-1);
		ImGui::ProgressBar(ImGui::GetTime() * -0.2f);
	}


	if (!Window::Render()) {
		ImGui::End();
		return false;
	}
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(_PROJECTNAME))
		{
			ImGui::MenuItem("Settings");
			if (ImGui::MenuItem("Keys Manager")) {
				keyManagerWindow = new KeyManagerWindow();
				keyManagerWindow->Show();
			}
			ImGui::MenuItem("About");
			ImGui::EndMenu();
		}
		if (selectedApp < 0)
			ImGui::BeginDisabled();
		if (ImGui::BeginMenu("Build")) {

			//ImGui::MenuItem("View keys");

			ImGui::EndMenu();
		}
		if (selectedApp < 0)
			ImGui::EndDisabled();
		ImGui::EndMenuBar();
	}

	Host* pSelectedHost = nullptr;
	App* pSelectedApp = nullptr;
	Build* pSelectedBuild = nullptr;

	ImGui::Columns(3);
	ImGui::Text("Hosts");
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##host")) {
		for (int i = 0; i < UpdateManager::GetHosts()->size(); i++) {
			if (ImGui::Selectable(UpdateManager::GetHosts()->at(i).Uri.c_str(), i == selectedHost)) {
				selectedHost = i;
				selectedApp = -1;
				selectedBuild = -1;
				selectedFile.clear();
			}
		}

		ImGui::EndListBox();
	}
	if (ImGui::Button("+##host", ImVec2(30, 0))) {
		ImGui::OpenPopup("Add Host");
	}
	ImGui::SameLine();
	// ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30 + ImGui::GetStyle().FramePadding.x + 2);
	if (ImGui::Button("-##host", ImVec2(30, 0))) {
		ImGui::OpenPopup("Remove Host##host");
	}

	ImGui::NextColumn();
	ImGui::Text(GetAppsText().c_str());
	disabled = selectedHost == -1 || UpdateManager::GetHosts()->at(selectedHost).WaitingGetApps;
	if (disabled)
		ImGui::BeginDisabled();
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##app")) {
		if (selectedHost > -1) {
			auto apps = UpdateManager::GetHosts()->at(selectedHost).GetApps();
			for (int i = 0; i < apps->size(); i++) {
				if (ImGui::Selectable(apps->at(i).Id.c_str(), i == selectedApp)) {
					selectedApp = i;
					selectedBuild = -1;
					selectedFile.clear();
				}
			}
		}

		ImGui::EndListBox();
	}

	//disabled = selectedHost == -1;// || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
	if (disabled && selectedHost != -1 && UpdateManager::GetHosts()->at(selectedHost).IsAdmin)
		ImGui::EndDisabled();
	if (ImGui::Button("+", ImVec2(30, 0))) {
		ImGui::OpenPopup("Add App##app");
	}
	if (disabled && selectedHost != -1 && UpdateManager::GetHosts()->at(selectedHost).IsAdmin)
		ImGui::BeginDisabled();
	ImGui::SameLine();
	if (ImGui::Button("-", ImVec2(30, 0))) {
		ImGui::OpenPopup("Remove App##app");
	}
	if (disabled)
		ImGui::EndDisabled();


	ImGui::NextColumn();
	disabled = selectedApp == -1 || UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).WaitingGetBuilds;
	if (disabled)
		ImGui::BeginDisabled();
	ImGui::Text(GetBuildsText().c_str());
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##build")) {
		if (selectedApp > -1) {
			auto builds = UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds();
			for (int i = 0; i < builds->size(); i++) {
				\
					UpdateManager::Build* build = &builds->at(i);
				if (!build->LastBuild)
					if (!build->App->Host->IsAdmin && !build->HasDetails())
						ImGui::BeginDisabled();
				if (ImGui::Selectable(build->Id.c_str(), i == selectedBuild)) {
					selectedBuild = i;
					selectedFile.clear();
				}
				const char* statusText;
				if (build->LastBuild) {
					statusText = "Last build";
					ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.18f, 0.8f, 0.443f, 1));
				}
				else
					if (build->App->Host->IsAdmin)
						statusText = "Available";
					else
						statusText = "Not available";
				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(statusText).x);
				ImGui::TextDisabled(statusText);
				if (build->LastBuild)
					ImGui::PopStyleColor();
				if (!build->LastBuild)
					if (!build->App->Host->IsAdmin && !build->HasDetails())
						ImGui::EndDisabled();
			}
		}
		ImGui::EndListBox();
	}
	if (disabled)
		ImGui::EndDisabled();

	disabled = selectedApp == -1 || UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).WaitingGetBuilds || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
	if (disabled)
	{
		ImGui::BeginDisabled();
	}

	ImGui::Button("+", ImVec2(30, 0));
	if (disabled)
	{
		ImGui::EndDisabled();
	}

	ImGui::Columns(1);

	ImGui::Text("Search by name");
	ImGui::SetNextItemWidth(-1);
	ImGui::InputText("##searchinput", searchBuffer, sizeof(searchBuffer));
	ImGui::BeginChild("##files", ImVec2(0, 400), ImGuiChildFlags_Border);
	{
		auto filesText = "Files";
		auto windowWidth = ImGui::GetContentRegionAvail().x;
		auto textWidth = ImGui::CalcTextSize(filesText).x;

		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f - ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::Text(filesText);
	}
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##files", ImVec2(0, ImGui::GetContentRegionAvail().y))) {
		if (selectedBuild > -1) {
			auto build = &UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild);
			auto files = build->GetFiles();
			for (int i = 0; i < files->size(); i++) {
				BuildFile* file = &files->at(i);
				string search = string(searchBuffer);
				if (search != "") {
					if (file->Name.find(search) == string::npos)
						continue;
				}

				if (!build->App->Host->IsAdmin && !build->LastBuild && !file->Downloaded) {
					ImGui::BeginDisabled();
				}

				if (ImGui::Selectable(file->Name.c_str(), std::find(selectedFile.begin(), selectedFile.end(), i) != selectedFile.end()))
				{
					if (GetAsyncKeyState(VK_CONTROL)) {
						if (std::find(selectedFile.begin(), selectedFile.end(), i) != selectedFile.end())
							selectedFile.erase(std::find(selectedFile.begin(), selectedFile.end(), i));
						else
							selectedFile.push_back(i);
					}
					else {
						selectedFile.clear();
						selectedFile.push_back(i);
					}
				}

				const char* text;
				if (!build->App->Host->IsAdmin && !build->LastBuild && !file->Downloaded) {
					text = "Not available";
				}
				else {
					if (file->Downloaded) {
						switch (file->LoadDepot(false)) {
						case BuildFile::LoadResult::Success: {
							switch (file->CheckDepot(false)) {
							case BuildFile::UnpackResult::Success: {
								text = "Unpacked";
								break;
							}
							case BuildFile::UnpackResult::NotUnpackedYet: {
								text = "Downloaded / Not unpacked yet";
								break;
							}
							case BuildFile::UnpackResult::KeyNotFound: {
								text = "Key not found";
								break;
							}
							case BuildFile::UnpackResult::LoadError: {
								text = "Load error";
								break;
							}
							default:
							{
								text = "Unknown error (Unpacking)";
								break;
							}
							}
							break;
						}
						case BuildFile::LoadResult::FileNotFound: {
							text = "File not found";
							break;
						}
						case BuildFile::LoadResult::UnknownFileType: {
							text = "Unknown file type";
							break;
						}
						default:
						{
							text = "Unknown error (Loading)";
							break;
						}
						}
					}
					else {
						text = "Available";
					}
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					// Do stuff on Selectable() double click.
					openingBuilds.push_back(file); // may cause misli

					DownloadingCount++;
					CreateThread(NULL, NULL, [](void* data) -> DWORD {
						UpdateManager::BuildFile* openingBuild = openingBuilds.back(); // may cause misleading if you click another file faster than CreateThread was called (lol just try)

						if (!openingBuild->Downloaded)
							openingBuild->DownloadDepot();
						DownloadingCount--;
						ViewWindow* openedWindow = nullptr;
						for (Window*& obj : DirectX::Windows) {
							ViewWindow* view = (ViewWindow*)obj;
							if (view->GetBuildFile() == openingBuild) {
								openedWindow = view;
								break;
							}
						}
						if (!openedWindow) {
							UnpackingProgresses[openingBuild] = make_pair(0, 1);
							if (openingBuild->UnpackDepot(&UnpackingProgresses[openingBuild].first, &UnpackingProgresses[openingBuild].second) == BuildFile::UnpackResult::Success) {
								ViewWindow* window = new ViewWindow(openingBuild);
								window->SetDock(dockId);
							}
							UnpackingProgresses.erase(UnpackingProgresses.find(openingBuild));
							openingBuilds.erase(std::find(openingBuilds.begin(), openingBuilds.end(), openingBuild));

							return 0;

						}
						else {
							openedWindow->Show();
						}
						}, NULL, NULL, NULL);
				}

				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(text).x);
				if (strcmp(text, "Key not found") == 0) { // I'm 2lazy to do this at CheckDepot ;(
					ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.91f, 0.30f, 0.24f, 1.f));
					ImGui::TextDisabled(text);
					ImGui::PopStyleColor();
				}
				else if (strcmp(text, "Unpacked") == 0) {
					ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.18f, 0.8f, 0.443f, 1));
					ImGui::TextDisabled(text);
					ImGui::PopStyleColor();
				}
				else {
					ImGui::TextDisabled(text);
				}

				if (!build->App->Host->IsAdmin && !build->LastBuild && !file->Downloaded) {
					ImGui::EndDisabled();
				}
			}
		}

		ImGui::EndListBox();
	}
	ImGui::EndChild();

	ImGui::Spacing();

	if (selectedBuild == -1)
		ImGui::BeginDisabled();
	ImGui::Button("Download all", ImVec2(ImGui::GetContentRegionAvail().x / 2 - style.FramePadding.x, 0));
	if (selectedBuild == -1)
		ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::Button("Open", ImVec2(ImGui::GetContentRegionAvail().x, 0));
	ImGui::Spacing();

	//ImGuiWindowClass windowClass;
	//windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_TopMost;
	//ImGui::SetNextWindowClass(&windowClass);
	if (ImGui::BeginPopupModal("Add App##app", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text(" App Name");
		ImGui::InputText("##appname", inputAppName, sizeof(inputAppName));

		if (UpdateManager::GetHosts()->at(selectedHost).IsAdmin) {
			ImGui::Text("Access Group");
			if (ImGui::BeginCombo("##addappaccessgroup", comboBoxAppAccessGroup.c_str())) {
				if (ImGui::Selectable("##addappemptyag", comboBoxAppAccessGroup == "")) {
					comboBoxAppAccessGroup = "";
				}
				for (auto& ag : UpdateManager::GetHosts()->at(selectedHost).accessGroup) {
					if (ImGui::Selectable(ag.first.c_str(), ag.first == comboBoxAppAccessGroup)) {
						comboBoxAppAccessGroup = ag.first;
					}
				}
				ImGui::EndCombo();
			}
		}

		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemInnerSpacing.x, 0))) {
			UpdateManager::GetHosts()->at(selectedHost).AddApp(string(inputAppName), comboBoxAppAccessGroup);
			ResetAddApp();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ResetAddApp();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Remove App##app", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text(" Do you want to remove app?");
		if (ImGui::Button("Remove", ImVec2(100, 0))) {
			UpdateManager::GetHosts()->at(selectedHost).RemoveApp(UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).Id);
			selectedApp = -1;
			selectedBuild = -1;
			selectedFile.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 0))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Add Host", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text(" Host");
		ImGui::InputText("##hostinput", inputHostNameBuffer, sizeof(inputHostNameBuffer));
		{
			auto filesText = "Use only SSL protoctol";
			auto windowWidth = ImGui::GetContentRegionAvail().x;
			auto textWidth = ImGui::CalcTextSize(filesText).x;

			ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f + 10);
			ImGui::TextDisabled(filesText);
			ImGui::Spacing();
			ImGui::Spacing();
		}
		ImGui::Checkbox("Show additional options", &modalHostAdditional);
		if (modalHostAdditional) {
			ImGui::BeginChild("##additionaloptions", ImVec2(0, 0), ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border, 0);
			ImGui::Checkbox("Is Admin?", &modalHostIsAdmin);
			ImGui::Text(" Login");
			if (!modalHostIsAdmin)
				ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##login", inputAdminLogin, sizeof(inputAdminLogin));
			if (!modalHostIsAdmin)
				ImGui::EndDisabled();
			ImGui::Text(" Password");
			if (!modalHostIsAdmin)
				ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##password", inputAdminPassword, sizeof(inputAdminPassword), ImGuiInputTextFlags_Password);
			if (!modalHostIsAdmin)
				ImGui::EndDisabled();

			ImGui::Text(" OS");
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##os", "Windows")) { // win | osx
				ImGui::Selectable("Windows", true);
				ImGui::BeginDisabled();
				ImGui::Selectable("Mac OS", false);
				ImGui::EndDisabled();
				ImGui::EndCombo();
			}
			ImGui::Text(" Stage");
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##stage", "Public")) { // public | canary | staging (encrypted)
				ImGui::Selectable("Public", true);
				ImGui::BeginDisabled();
				ImGui::Selectable("Canary", false);
				ImGui::Selectable("Staging", false);
				ImGui::EndDisabled();
				ImGui::EndCombo();
			}
			ImGui::EndChild();
		}
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemInnerSpacing.x, 0))) {
			UpdateManager::AddHost(string(inputHostNameBuffer), modalHostIsAdmin, string(inputAdminLogin), string(inputAdminPassword));
			//UpdateManager::GetHosts(true);
			ImGui::CloseCurrentPopup();
			ResetAddHost();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ResetAddHost();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Remove Host##host", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text(" Do you want to remove host?");
		if (ImGui::Button("Remove", ImVec2(100, 0))) {
			UpdateManager::RemoveHost(UpdateManager::Hosts[selectedHost].Uri);
			selectedHost = -1;
			selectedApp = -1;
			selectedBuild = -1;
			selectedFile.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 0))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::End();

	return true;
}
