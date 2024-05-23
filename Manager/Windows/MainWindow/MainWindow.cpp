#include "MainWindow.h"

int selectedHost = -1;
int selectedApp = -1;
int selectedBuild = -1;
std::vector<int> selectedFile;

int DownloadingCount;
std::map<UpdateManager::BuildFile*, std::pair<int, int>> UnpackingProgresses; // Build > (Progress, MaxProgress)

std::vector<UpdateManager::BuildFile*> openingBuilds;

bool MainWindow::Render()
{
	bool disabled = false;

	ImGui::Begin("Manager", &this->Opened, ImGuiWindowFlags_MenuBar);
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
			ImGui::MenuItem("Keys Manager");
			ImGui::MenuItem("About");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Build")) {

			ImGui::MenuItem("View keys");

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::Columns(3);

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
	ImGui::Button("+", ImVec2(30, 0));
	ImGui::NextColumn();
	ImGui::PushItemWidth(-1);
	if (ImGui::BeginListBox("##app")) {
		if (selectedHost > -1) {
			auto versions = UpdateManager::GetHosts()->at(selectedHost).GetVersions();
			for (int i = 0; i < versions->size(); i++) {
				if (ImGui::Selectable(versions->at(i).Id.c_str(), i == selectedApp)) {
					selectedApp = i;
					selectedBuild = -1;
					selectedFile.clear();
				}
			}
		}

		ImGui::EndListBox();
	}
	disabled = selectedHost == -1 || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
	if (disabled)
	{
		ImGui::BeginDisabled();
	}

	ImGui::Button("+", ImVec2(30, 0));
	if (disabled)
	{
		ImGui::EndDisabled();
	}
	ImGui::NextColumn();
	ImGui::PushItemWidth(-1);

	if (ImGui::BeginListBox("##build")) {
		if (selectedApp > -1) {
			auto builds = UpdateManager::GetHosts()->at(selectedHost).GetVersions()->at(selectedApp).GetBuilds();
			for (int i = 0; i < builds->size(); i++) {
				auto build = builds->at(i);
				if (build.LastBuild)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f, 0.8f, 0.443f, 1)); // #2ecc71
				else
					if (!build.App->Host->IsAdmin)
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.584f, 0.647f, 0.651f, 1)); // #95a5a6
				if (ImGui::Selectable(build.Id.c_str(), i == selectedBuild)) {
					selectedBuild = i;
					selectedFile.clear();
				}
				if (build.LastBuild)
					ImGui::PopStyleColor();
				else
					if (!build.App->Host->IsAdmin)
						ImGui::PopStyleColor();
			}
		}
		ImGui::EndListBox();
	}

	disabled = selectedApp == -1 || !UpdateManager::GetHosts()->at(selectedHost).IsAdmin;
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
	ImGui::PushItemWidth(-1);

	if (ImGui::BeginListBox("##files", ImVec2(0, 300))) {
		if (selectedBuild > -1) {
			auto build = &UpdateManager::GetHosts()->at(selectedHost).GetVersions()->at(selectedApp).GetBuilds()->at(selectedBuild);
			auto files = build->GetFiles();
			for (int i = 0; i < files->size(); i++) {
				if (!build->App->Host->IsAdmin && !build->LastBuild && !files->at(i).Downloaded) {
					ImGui::BeginDisabled();
				}

				if (ImGui::Selectable(files->at(i).Name.c_str(), std::find(selectedFile.begin(), selectedFile.end(), i) != selectedFile.end()))
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
				if (!build->App->Host->IsAdmin && !build->LastBuild && !files->at(i).Downloaded) {
					text = "Not available";
				}
				else {
					if (files->at(i).Downloaded) {
						switch (files->at(i).LoadDepot(false)) {
						case BuildFile::LoadResult::Success: {
							switch (files->at(i).CheckDepot(false)) {
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
					openingBuilds.push_back(&files->at(i)); // may cause misli

					DownloadingCount++;
					CreateThread(NULL, NULL, [](void* data) -> DWORD {
						UpdateManager::BuildFile* openingBuild = openingBuilds.back(); // may cause misleading if you click another file faster than CreateThread was called (lol just try)

						ImGuiID dockId = ImGui::GetID("viewwindows_dock");
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
							openingBuild->UnpackDepot(&UnpackingProgresses[openingBuild].first, &UnpackingProgresses[openingBuild].second);
							UnpackingProgresses.erase(UnpackingProgresses.find(openingBuild));
							openingBuilds.erase(std::find(openingBuilds.begin(), openingBuilds.end(), openingBuild));
							ViewWindow* window = new ViewWindow(openingBuild);
							window->SetDock(dockId);
							return 0;

						}
						else {
							openedWindow->Show();
						}
						}, NULL, NULL, NULL);
				}

				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(text).x);
				ImGui::BeginDisabled();
				ImGui::Text(text);
				ImGui::EndDisabled();

				if (!build->App->Host->IsAdmin && !build->LastBuild && !files->at(i).Downloaded) {
					ImGui::EndDisabled();
				}
			}

		}

		ImGui::EndListBox();
	}

	ImGui::Spacing();

	ImGui::Button("Open", ImVec2(-1, 0));
	ImGui::Spacing();
	//ImGui::PushItemWidth(-1);
	//ImGui::ProgressBar(0);
	ImGui::End();

	return true;
}
