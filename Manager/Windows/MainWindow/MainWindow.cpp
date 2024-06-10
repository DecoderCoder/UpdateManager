#include "MainWindow.h"

char searchBuffer[256] = { 0 };

char inputHostNameBuffer[256] = { 0 };
char inputAdminLoginBuffer[256] = { 0 };
char inputAdminPasswordBuffer[256] = { 0 };
char inputAppNameBuffer[256] = { 0 };
char inputBuildNameBuffer[256] = { 0 };
char inputDepotNameBuffer[256] = { 0 };
string comboBoxAppAccessGroup;

int selectedHost = -1;
int selectedApp = -1;
int selectedBuild = -1;
std::vector<int> selectedDepots;

int DownloadingCount;
std::map<UpdateManager::BuildDepot*, std::pair<int, int>> UnpackingProgresses; // Build > (Progress, MaxProgress)
std::vector<UpdateManager::BuildDepot*> openingBuilds;
ImGuiID dockId;

int removeOnServer = 0;
bool modalHostAdditional = false;
bool modalHostIsAdmin = false;

void ResetAddHost() {
	modalHostIsAdmin = false;
	memset(inputHostNameBuffer, 0, sizeof(inputHostNameBuffer));
	memset(inputAdminLoginBuffer, 0, sizeof(inputAdminLoginBuffer));
	memset(inputAdminPasswordBuffer, 0, sizeof(inputAdminPasswordBuffer));
}

void ResetAddApp() {
	memset(inputAppNameBuffer, 0, sizeof(inputAppNameBuffer));
	comboBoxAppAccessGroup = "";
}

void ResetBuildAdd() {
	memset(inputBuildNameBuffer, 0, sizeof(inputBuildNameBuffer));
	comboBoxAppAccessGroup = "";
}

void ResetDepotAdd() {
	memset(inputDepotNameBuffer, 0, sizeof(inputDepotNameBuffer));
}

const int dotsDelay = 300;
std::pair<char, int> appsDots = make_pair(0, 0);
std::pair<char, int> buildsDots = make_pair(0, 0);

std::mutex obj;
std::vector<HANDLE> threads;
bool downloadOrUpload = false; // false = download | true = upload
int processingLeft = 0;
std::vector<UpdateManager::BuildDepot>* processingDepots = nullptr; // depots | left

void processDepotsThread() {
	while (processingDepots && processingDepots->size() > 0) {
		BuildDepot depot;

		obj.lock();
		if (processingDepots->size() == 0)
		{
			obj.unlock();
			break;
		}
		depot = processingDepots->at(0);
		processingDepots->erase(processingDepots->begin());
		obj.unlock();

		if (downloadOrUpload)
		{
			if (depot.PackDepot())
				depot.UploadDepot();
		}
		else {
			depot.DownloadDepot();
		}
		processingLeft--;
	}
}

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

bool openSettings = false;

void OpenSelected() {
	auto depots = UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild).GetDepots();
	for (int i = 0; i < selectedDepots.size(); i++) {
		openingBuilds.push_back(&depots->at(selectedDepots[i])); // may cause misli
		DownloadingCount++;
		CreateThread(NULL, NULL, [](void* data) -> DWORD {
			UpdateManager::BuildDepot* openingBuild = openingBuilds.back(); // may cause misleading if you click another file faster than CreateThread was called (lol just try)

			if (!openingBuild->Downloaded && openingBuild->OnServer)
				openingBuild->DownloadDepot();
			DownloadingCount--;
			ViewWindow* openedWindow = nullptr;
			for (Window*& obj : DirectX::Windows) {
				ViewWindow* view = (ViewWindow*)obj;
				if (view->GetBuildDepot() == openingBuild) {
					openedWindow = view;
					break;
				}
			}
			if (!openedWindow) {
				UnpackingProgresses[openingBuild] = make_pair(0, 1);
				if (!openingBuild->OnServer || openingBuild->UnpackDepot(&UnpackingProgresses[openingBuild].first, &UnpackingProgresses[openingBuild].second) == BuildDepot::UnpackResult::Success) {
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
}

bool MainWindow::Render()
{
	auto style = &ImGui::GetStyle();
	if (Settings::DarkMode) {
		style->FrameBorderSize = 0;
		ImGui::StyleColorsDark();
	}
	else
	{
		ImGui::StyleColorsLight();
		style->FrameBorderSize = 1;
	}

	style->WindowMinSize = ImVec2(650, 500);
	style->WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style->AntiAliasedFill = true;
	style->AntiAliasedLines = true;
	style->AntiAliasedLinesUseTex = true;

	bool disableAll = false;
	bool disabled = false;

	if (ImGui::GetTopMostPopupModal()) {
		//ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));
		const char* text = "If a window stuck behind another window\r\nPress ESC";
		auto textSize = ImGui::CalcTextSize(text);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
		ImGui::SetNextWindowPos(ImVec2(GetSystemMetrics(SM_CXSCREEN) / 2 - textSize.x / 2 - style->FramePadding.x, 30));
		ImGui::SetNextWindowSize(ImVec2(textSize.x + style->FramePadding.x * 2 + style->WindowPadding.y * 2, textSize.y + style->WindowPadding.y * 2 + style->FramePadding.y * 2 + ImGui::GetFontSize() + style->FramePadding.y * 2));
		ImGui::Begin("Stuck Helper##stuckWindow", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs);
		ImGui::Text(text);
		ImGui::End();
		ImGui::PopStyleVar();
		//ImGui::PopStyleColor();
	}


	ImGui::Begin(_PROJECTNAME, &this->Opened, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking);
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

	if (processingLeft) {
		disableAll = true;
		float count = UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild).GetDepots()->size();

		ImGui::ProgressBar((count - (float)processingLeft) / (float)count);

		if (processingDepots != nullptr && processingDepots->size() == 0) {
			delete(processingDepots);
			processingDepots = nullptr;
		}
	}


	if (!Window::Render()) {
		ImGui::End();
		return false;
	}

	if (openSettings) {
		ImGui::OpenPopup("Settings##mainwindow");
		openSettings = false;
	}

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(_PROJECTNAME))
		{
			if (processingLeft)
				ImGui::BeginDisabled();
			if (ImGui::MenuItem("Settings")) {
				openSettings = true;
			}
			if (processingLeft)
				ImGui::EndDisabled();
			if (ImGui::MenuItem("Keys Manager")) {
				KeyManagerWindow* keyManagerWindow = nullptr;
				for (Window*& obj : DirectX::Windows) {
					KeyManagerWindow* view = dynamic_cast<KeyManagerWindow*>(obj);
					if (view) {
						keyManagerWindow = (KeyManagerWindow*)obj;
						break;
					}
				}
				if (!keyManagerWindow) {
					keyManagerWindow = new KeyManagerWindow();
					keyManagerWindow->Show();
				}
				else {
					keyManagerWindow->Show();
				}
			}
			ImGui::Separator();
			//if (ImGui::MenuItem("Change theme")) {
			//	Settings::darkMode = !Settings::darkMode;
			//}
			if (ImGui::MenuItem("About")) {
				AboutWindow* aboutWindow = nullptr;
				for (Window*& obj : DirectX::Windows) {
					AboutWindow* view = dynamic_cast<AboutWindow*>(obj);
					if (view) {
						aboutWindow = (AboutWindow*)obj;
						break;
					}
				}
				if (!aboutWindow) {
					aboutWindow = new AboutWindow();
					aboutWindow->Show();
				}
				else {
					aboutWindow->Show();
				}
			}
			ImGui::EndMenu();
		}
		if (selectedApp < 0)
			ImGui::BeginDisabled();
		if (ImGui::BeginMenu("App")) {

			if (ImGui::MenuItem("Build updater")) {

			}

			ImGui::EndMenu();
		}
		if (selectedApp < 0)
			ImGui::EndDisabled();
		ImGui::EndMenuBar();
	}

	Host* pSelectedHost = nullptr;
	App* pSelectedApp = nullptr;
	Build* pSelectedBuild = nullptr;

	ImGui::Columns(3, 0, false);
	if (disableAll)
		ImGui::BeginDisabled();

	if (selectedHost != -1 && UpdateManager::GetHosts()->at(selectedHost).IsAdmin)
		ImGui::Text("Hosts [ADMIN]");
	else
		ImGui::Text("Hosts");
	ImGui::PushItemWidth(-1);

	if (ImGui::BeginListBox("##host")) {
		for (int i = 0; i < UpdateManager::GetHosts()->size(); i++) {
			if (ImGui::Selectable(UpdateManager::GetHosts()->at(i).Uri.c_str(), i == selectedHost)) {
				selectedHost = i;
				selectedApp = -1;
				selectedBuild = -1;
				selectedDepots.clear();
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
	if (disableAll)
		ImGui::EndDisabled();
	ImGui::NextColumn();
	disabled = selectedHost == -1 || UpdateManager::GetHosts()->at(selectedHost).WaitingGetApps;
	if (disableAll || disabled)
		ImGui::BeginDisabled();
	ImGui::Text(GetAppsText().c_str());
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##app")) {
		if (selectedHost > -1) {
			auto apps = UpdateManager::GetHosts()->at(selectedHost).GetApps();
			for (int i = 0; i < apps->size(); i++) {
				if (ImGui::Selectable(apps->at(i).Id.c_str(), i == selectedApp)) {
					selectedApp = i;
					selectedBuild = -1;
					selectedDepots.clear();
				}
			}
		}

		ImGui::EndListBox();
	}

	//disabled = selectedHost == -1;// || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
	if (disableAll || disabled && selectedHost != -1 && UpdateManager::GetHosts()->at(selectedHost).IsAdmin)
		ImGui::EndDisabled();
	if (disableAll)
		ImGui::BeginDisabled();
	if (ImGui::Button("+##app", ImVec2(30, 0))) {
		ImGui::OpenPopup("Add App##app");
	}
	if (disableAll)
		ImGui::EndDisabled();
	if (disableAll || disabled && selectedHost != -1 && UpdateManager::GetHosts()->at(selectedHost).IsAdmin)
		ImGui::BeginDisabled();
	ImGui::SameLine();
	if (disableAll || selectedApp != -1 && UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).WaitingGetBuilds)
		ImGui::BeginDisabled();
	if (ImGui::Button("-##app", ImVec2(30, 0))) {
		ImGui::OpenPopup("Remove App##app");
	}
	if (disableAll || selectedApp != -1 && UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).WaitingGetBuilds)
		ImGui::EndDisabled();
	if (disableAll || disabled)
		ImGui::EndDisabled();


	ImGui::NextColumn();
	disabled = selectedApp == -1 || UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).WaitingGetBuilds;
	if (disableAll || disabled)
		ImGui::BeginDisabled();
	ImGui::Text(GetBuildsText().c_str());
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##build")) {
		if (selectedApp > -1) {
			auto builds = UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds();
			for (int i = 0; i < builds->size(); i++) {
				UpdateManager::Build* build = &builds->at(i);
				if (!build->LastBuild)
					if (!build->App->Host->IsAdmin && !build->HasDetails())
						ImGui::BeginDisabled();
				if (ImGui::Selectable(build->Id.c_str(), i == selectedBuild)) {
					selectedBuild = i;
					selectedDepots.clear();
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
	if (disableAll || disabled)
		ImGui::EndDisabled();

	disabled = selectedApp == -1 || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
	if (disableAll || disabled)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("+", ImVec2(30, 0))) {
		ImGui::OpenPopup("Add Build##build");
	}

	if (disableAll || disabled)
	{
		ImGui::EndDisabled();
	}

	ImGui::Columns(1);

	ImGui::Text("Search by name");
	ImGui::SetNextItemWidth(-1);
	ImGui::InputText("##searchinput", searchBuffer, sizeof(searchBuffer));
	ImGui::BeginChild("##depots", ImVec2(0, ImGui::GetContentRegionAvail().y - 35), ImGuiChildFlags_Border);
	{
		auto depotsText = "Depots";
		auto windowWidth = ImGui::GetContentRegionAvail().x;
		auto textWidth = ImGui::CalcTextSize(depotsText).x;

		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f - ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::Text(depotsText);
	}

	ImGui::PushItemWidth(-1);
	if (disableAll)
		ImGui::BeginDisabled();
	if (ImGui::BeginListBox("##depotslist", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::CalcTextSize("abc").y - style->FramePadding.y * 2 - style->ItemSpacing.y))) {
		if (selectedBuild > -1) {
			auto build = &UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild);
			auto depots = build->GetDepots();
			for (int i = 0; i < depots->size(); i++) {
				BuildDepot* depot = &depots->at(i);
				string search = string(searchBuffer);
				if (search != "") {
					if (depot->Name.find(search) == string::npos)
						continue;
				}

				if (!build->App->Host->IsAdmin && !build->LastBuild && !depot->Downloaded) {
					ImGui::BeginDisabled();
				}

				if (ImGui::Selectable(depot->Name.c_str(), std::find(selectedDepots.begin(), selectedDepots.end(), i) != selectedDepots.end()))
				{
					//if (GetAsyncKeyState(VK_CONTROL)) { // remove it 
					//	if (std::find(selectedDepots.begin(), selectedDepots.end(), i) != selectedDepots.end())
					//		selectedDepots.erase(std::find(selectedDepots.begin(), selectedDepots.end(), i));
					//	else
					//		selectedDepots.push_back(i);
					//}
					//else 
					{
						selectedDepots.clear();
						selectedDepots.push_back(i);
					}
				}

				const char* text;
				if (!build->App->Host->IsAdmin && !build->LastBuild && !depot->Downloaded) {
					text = "Not available";
				}
				else {
					if (depot->Downloaded) {
						switch (depot->LoadDepot(false, Settings::LowRAM)) {
						case BuildDepot::LoadResult::Success: {
							switch (depot->CheckDepot(false)) {
							case BuildDepot::UnpackResult::Success: {
								text = "Unpacked";
								break;
							}
							case BuildDepot::UnpackResult::NotUnpackedYet: {
								text = "Downloaded / Not unpacked yet";
								break;
							}
							case BuildDepot::UnpackResult::KeyNotFound: {
								text = "Key not found";
								break;
							}
							case BuildDepot::UnpackResult::LoadError: {
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
						case BuildDepot::LoadResult::FileNotFound: {
							text = "File not found";
							break;
						}
						case BuildDepot::LoadResult::UnknownFileType: {
							text = "Unknown file type";
							break;
						}
						default:
						{
							text = "Unknown error (Loading)";
							break;
						}
						}
						//if (Settings::LowRAMMode) {
						//	depot->UnloadDepot();
						//}
					}
					else {
						text = "Available";
					}
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					// Do stuff on Selectable() double click.
					OpenSelected();
				}


				if (UpdateManager::GetHosts()->at(selectedHost).IsAdmin && !depot->OnServer) {
					ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Local depot").x);
					ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.945, 0.769, 0.059, 1.f));
					ImGui::TextDisabled("Local depot");
					ImGui::PopStyleColor();
				}
				else {
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
				}

				if (!build->App->Host->IsAdmin && !build->LastBuild && !depot->Downloaded) {
					ImGui::EndDisabled();
				}
			}
		}

		ImGui::EndListBox();
	}
	if (disableAll)
		ImGui::EndDisabled();
	if (disableAll || selectedBuild == -1)
		ImGui::BeginDisabled();
	if (ImGui::Button("Add depot", ImVec2(ImGui::GetContentRegionAvail().x / 2 - style->FramePadding.x, 0))) {
		ImGui::OpenPopup("Add Depot##depot");
	}
	if (disableAll || selectedBuild == -1)
		ImGui::EndDisabled();
	ImGui::SameLine();

	disabled = !selectedDepots.size();
	if (disableAll || disabled)
		ImGui::BeginDisabled();
	if (ImGui::Button("Remove local depot", ImVec2(ImGui::GetContentRegionAvail().x / 2 - style->FramePadding.x, 0))) {
		ImGui::OpenPopup("Remove Depot##depot");
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove depot on the server", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
		removeOnServer = GetTickCount() + 3000;
		ImGui::OpenPopup("Remove Depot##depot", true);
	}
	if (disableAll || disabled)
		ImGui::EndDisabled();

	if (ImGui::BeginPopupModal("Remove Depot##depot", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		auto depots = UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild).GetDepots();
		if (removeOnServer)
			ImGui::Text(" Are you sure you want to\r\n remove selected depot(s)\r\n on the server?");
		else
			ImGui::Text(" Are you sure you want to\r\n remove selected depot(s)?");
		string removeButtonText = "Remove";
		if (removeOnServer) {
			int timeLeft = removeOnServer - GetTickCount();
			timeLeft = max(0, timeLeft);
			if (timeLeft)
				removeButtonText += " (" + to_string(timeLeft / 1000 + 1) + ")";
			disabled = timeLeft;
		}
		if (disabled)
			ImGui::BeginDisabled();
		if (ImGui::Button(removeButtonText.c_str(), ImVec2(100, 0))) {
			for (auto obj : selectedDepots) {
				UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild).RemoveDepot(depots->at(obj).Name, removeOnServer);
			}

			selectedDepots.clear();
			ImGui::CloseCurrentPopup();
			removeOnServer = 0;
		}
		if (disabled)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 0))) {
			ImGui::CloseCurrentPopup();
			removeOnServer = 0;
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Add Depot##depot", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		string inputDepotName = string(inputDepotNameBuffer);
		ImGui::Text(" Depot Name");
		ImGui::InputText("##depotname", inputDepotNameBuffer, sizeof(inputDepotNameBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, [](ImGuiInputTextCallbackData* data) { if (data->EventChar >= 'a' && data->EventChar <= 'z' || data->EventChar >= 'A' && data->EventChar <= 'Z' || data->EventChar >= '0' && data->EventChar <= '9') return 0; return 1; });

		disabled = inputDepotName.size() == 0;
		if (disabled)
			ImGui::BeginDisabled();
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemInnerSpacing.x, 0))) {

			UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild).AddDepot(inputDepotName);
			ImGui::CloseCurrentPopup();
		}
		if (disabled)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ResetDepotAdd();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::EndChild();

	ImGui::Spacing();

	if (disableAll || selectedBuild == -1)
		ImGui::BeginDisabled();

	if (ImGui::Button("Download all", ImVec2(ImGui::GetContentRegionAvail().x / 3 - style->FramePadding.x, 0))) {
		downloadOrUpload = false;
		auto depots = new std::vector<UpdateManager::BuildDepot>(*UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild).GetDepots());

		processingDepots = depots;

		for (int i = 0; i < min(depots->size(), Settings::ThreadsCount); i++) {
			threads.push_back(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)processDepotsThread, 0, 0, 0));
		}
	}

	if (disableAll || selectedBuild == -1)
		ImGui::EndDisabled();
	ImGui::SameLine();
	disabled = selectedBuild == -1 || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
	if (disableAll || disabled)
		ImGui::BeginDisabled();
	if (ImGui::Button("Upload all", ImVec2(ImGui::GetContentRegionAvail().x / 2 - style->FramePadding.x, 0)))
	{
		downloadOrUpload = true;
		auto depots = new std::vector<UpdateManager::BuildDepot>(*UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).GetBuilds()->at(selectedBuild).GetDepots());

		processingDepots = depots;
		processingLeft = processingDepots->size();
		for (int i = 0; i < min(depots->size(), Settings::ThreadsCount); i++) {
			threads.push_back(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)processDepotsThread, 0, 0, 0));
		}
	}
	if (disableAll || disabled)
		ImGui::EndDisabled();
	ImGui::SameLine();
	if (disableAll || selectedDepots.size() == -1)
		ImGui::BeginDisabled();
	if (ImGui::Button("Open", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
		OpenSelected();
	}
	if (disableAll)
		ImGui::EndDisabled();
	ImGui::Spacing();

	//ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopupModal("Settings##mainwindow", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		ImGui::Checkbox("Low RAM mode", &Settings::LowRAM);
		ImGui::SetItemTooltip("Programm will not hold depots in memory (may cause GUI freeze)");
		ImGui::Checkbox("Dark Mode", &Settings::DarkMode);
		ImGui::BeginDisabled();
		ImGui::Checkbox("Use SSL", &Settings::UseSSL);
		ImGui::EndDisabled();
		ImGui::SliderInt("Threads count", &Settings::ThreadsCount, 1, 32);

#ifdef _DEBUG
		ImGui::Checkbox("Show ImGui Demo", &Settings::ShowImGuiDemoWindow);
#endif

		ImGui::BeginChild("Admin#settings", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border);

		//ImGui::Checkbox("Ask about downloading actual version", &Settings::Admin::AskDownloadNew);

		ImGui::EndChild();

		if (ImGui::Button("Save", ImVec2(-1, 0))) {
			Settings::SaveSettings();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Add Build##build", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		bool close = false;
		ImGui::Text(" Build Id");
		ImGui::InputText("##buildname", inputBuildNameBuffer, sizeof(inputBuildNameBuffer), ImGuiInputTextFlags_CharsDecimal);

		string inputBuildName = string(inputBuildNameBuffer);

		auto app = &UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp);
		auto builds = app->GetBuilds();
		disabled = !(inputBuildName.size() > 0 && (!app->Builds.size() || std::stoi(builds->at(builds->size() - 1).Id) < atoi(inputBuildNameBuffer)));
		if (disabled)
			ImGui::BeginDisabled();
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemInnerSpacing.x, 0))) {
			app->AddBuild(inputBuildName);
			ResetBuildAdd();
			close = true;
		}
		if (disabled)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ResetBuildAdd();
			close = true;
		}
		if (close)
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Add App##app", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		bool close = false;

		ImGui::Text(" App Name");
		ImGui::InputText("##appname", inputAppNameBuffer, sizeof(inputAppNameBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, [](ImGuiInputTextCallbackData* data) { if (data->EventChar >= 'a' && data->EventChar <= 'z' || data->EventChar >= 'A' && data->EventChar <= 'Z' || data->EventChar >= '0' && data->EventChar <= '9') return 0; return 1; });

		if (UpdateManager::GetHosts()->at(selectedHost).IsAdmin) {
			ImGui::Text("Access Group");
			if (ImGui::BeginCombo("##addappaccessgroup", comboBoxAppAccessGroup.c_str())) {
				if (ImGui::Selectable("##addappemptyag", comboBoxAppAccessGroup == "")) {
					comboBoxAppAccessGroup = "";
				}
				for (auto& ag : UpdateManager::GetHosts()->at(selectedHost).accessGroups) {
					if (ImGui::Selectable(ag->Value.c_str(), ag->Value == comboBoxAppAccessGroup)) {
						comboBoxAppAccessGroup = ag->Value;
					}
				}
				ImGui::EndCombo();
			}
		}
		string inputAppName = string(inputAppNameBuffer);

		if (inputAppName.size() == 0)
			ImGui::BeginDisabled();
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemInnerSpacing.x, 0))) {
			switch (UpdateManager::GetHosts()->at(selectedHost).AddApp(inputAppName, comboBoxAppAccessGroup)) {
			case UpdateManager::Host::AddAppResponse::HasDeleted: {
				ImGui::OpenPopup("Already exists##app2");
				break;
			}
			case UpdateManager::Host::AddAppResponse::AlreadyExists: {
				ResetAddApp();
				ImGui::OpenPopup("Already Exists##app");
				break;
			}
			default: {
				ResetAddApp();
				close = true;
				break;
			}
			}
		}
		if (inputAppName.size() == 0)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ResetAddApp();
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::BeginPopupModal("Already exists##app2", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
			if (GetAsyncKeyState(VK_ESCAPE))
				ImGui::CloseCurrentPopup();
			ImGui::Text(" App with this name is deleted.\r\nDo you want to restore it or create new?");
			if (ImGui::Button("Restore", ImVec2(-1, 0))) {
				UpdateManager::GetHosts()->at(selectedHost).AddApp(inputAppName, comboBoxAppAccessGroup, 0);
				ImGui::CloseCurrentPopup();
				close = true;
			}

			if (ImGui::Button("Create new", ImVec2(-1, 0))) {
				UpdateManager::GetHosts()->at(selectedHost).AddApp(inputAppName, comboBoxAppAccessGroup, 1);
				ImGui::CloseCurrentPopup();
				close = true;
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Already Exists##app", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
			if (GetAsyncKeyState(VK_ESCAPE))
				ImGui::CloseCurrentPopup();
			ImGui::Text("App with this name already exists");
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (close)
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Remove App##app", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		ImGui::Text(" Are you sure you want to remove app?");
		if (ImGui::Button("Remove", ImVec2(100, 0))) {
			UpdateManager::GetHosts()->at(selectedHost).RemoveApp(UpdateManager::GetHosts()->at(selectedHost).GetApps()->at(selectedApp).Id);
			selectedApp = -1;
			selectedBuild = -1;
			selectedDepots.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 0))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Add Host", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		ImGui::Text(" Host");
		ImGui::InputText("##hostinput", inputHostNameBuffer, sizeof(inputHostNameBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, [](ImGuiInputTextCallbackData* data) { if (data->EventChar >= 'a' && data->EventChar <= 'z' || data->EventChar >= 'A' && data->EventChar <= 'Z' || data->EventChar >= '0' && data->EventChar <= '9' || data->EventChar == '-' || data->EventChar == '_' || data->EventChar == '.') return 0; return 1; });
		{
			const char* depotsText;
			if (Settings::UseSSL)
				depotsText = "Use only SSL protoctol";
			else
				depotsText = " ";
			auto windowWidth = ImGui::GetContentRegionAvail().x;
			auto textWidth = ImGui::CalcTextSize(depotsText).x;

			ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f + 10);
			ImGui::TextDisabled(depotsText);
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
			ImGui::InputText("##login", inputAdminLoginBuffer, sizeof(inputAdminLoginBuffer));
			if (!modalHostIsAdmin)
				ImGui::EndDisabled();
			ImGui::Text(" Password");
			if (!modalHostIsAdmin)
				ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##password", inputAdminPasswordBuffer, sizeof(inputAdminPasswordBuffer), ImGuiInputTextFlags_Password);
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
		string inputHostName = string(inputHostNameBuffer);
		if (inputHostName.size() == 0)
			ImGui::BeginDisabled();
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemInnerSpacing.x, 0))) {
			UpdateManager::AddHost(inputHostName, modalHostIsAdmin, string(inputAdminLoginBuffer), string(inputAdminPasswordBuffer));
			//UpdateManager::GetHosts(true);
			ImGui::CloseCurrentPopup();
			ResetAddHost();
		}
		if (inputHostName.size() == 0)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ResetAddHost();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Remove Host##host", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
		if (GetAsyncKeyState(VK_ESCAPE))
			ImGui::CloseCurrentPopup();
		ImGui::Text(" Do you want to remove host?");
		if (ImGui::Button("Remove", ImVec2(100, 0))) {
			UpdateManager::RemoveHost(UpdateManager::Hosts[selectedHost].Uri);
			selectedHost = -1;
			selectedApp = -1;
			selectedBuild = -1;
			selectedDepots.clear();
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
