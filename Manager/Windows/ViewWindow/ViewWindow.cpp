#include "ViewWindow.h"
#include "../../DirectX/D3DX11tex.h"

bool ViewWindow::IsSelectedChanged() {	// to prevent code dup // upd: imgui has bug that SetNextWindowFocus not working in some cases
	bool temp = selectedChanged;
	if (selectedChanged)
		selectedChanged = false;
	return temp;
}

void ViewWindow::ParseFiles(wstring path, DirectoryNode* parentNode) {
	if (!fs::exists(path))
		return;
	parentNode->FullPath = path;
	parentNode->FileName = fs::path(path).filename().wstring();
	parentNode->IsDirectory = true;
	std::vector<fs::directory_entry> files;
	{
		auto filesTemp = fs::directory_iterator(path);
		for (const auto& obj : filesTemp) {
			if (fs::is_directory(obj))
				files.push_back(obj);
		}
		filesTemp = fs::directory_iterator(path);
		for (const auto& obj : filesTemp) {
			if (!fs::is_directory(obj))
				files.push_back(obj);
		}
	}
	for (auto obj : files) {
		fs::path objPath = obj.path();
		DirectoryNode* newNode = new DirectoryNode();
		newNode->FullPath = objPath.wstring();
		newNode->FileName = objPath.filename().wstring();

		if (fs::is_directory(objPath)) {
			newNode->IsDirectory = true;
			ParseFiles(objPath.wstring(), newNode);
			parentNode->Children.push_back(newNode);
		}
		else {
			LoadedFile* loadedFile = new LoadedFile(); // memory leak, but idc ;D
			loadedFile->FullPath = objPath.wstring();

			string ext = objPath.extension().string();

			if (ext == ".txt")
				loadedFile->FileType = LoadedFileType::Text;
			else if (ext == ".json")
				loadedFile->FileType = LoadedFileType::Text;
			else if (ext == ".xml")
				loadedFile->FileType = LoadedFileType::Text;
			else if (ext == ".html")
				loadedFile->FileType = LoadedFileType::Text;
			else if (ext == ".png")
				loadedFile->FileType = LoadedFileType::Image;
			else if (ext == ".jpg")
				loadedFile->FileType = LoadedFileType::Image;
			else if (ext == ".jpeg")
				loadedFile->FileType = LoadedFileType::Image;
			else
				loadedFile->FileType = LoadedFileType::Binary;


			ReadBinaryFile(loadedFile->FullPath, &loadedFile->Binary, loadedFile->BinarySize);
			loadedFile->Text = string(loadedFile->Binary, loadedFile->BinarySize);

			if (*(BuildFile::DFileType*)loadedFile->Text.data() == BuildFile::DFileType::Encrypted || *(BuildFile::DFileType*)loadedFile->Text.data() == BuildFile::DFileType::EncryptedFile)
				loadedFile->FileType = LoadedFileType::Encrypted;

			loadedFile->BinaryHex = ToHex(loadedFile->Binary, loadedFile->BinarySize, true);


			auto a = *(int*)loadedFile->Text.data() & (int)BuildFile::DFileType::Encrypted | (int)BuildFile::DFileType::EncryptedFile;

			//if (loadedFile->FileType == LoadedFileType::Image) {
			DirectX::LoadTextureFromFile(loadedFile->FullPath, loadedFile->Image);
			//}

			newNode->LoadedFile = loadedFile;

			parentNode->Children.push_back(newNode);
			if (!selectedFile)
				selectedFile = parentNode->Children.at(parentNode->Children.size() - 1);
		}

	}
}

ViewWindow::ViewWindow(wstring buildFilePath, UpdateManager::Build* build) : Window()
{
	/*BuildFile newBuildFile;
	newBuildFile.Build = newBuild;
	newBuildFile.Name = d["name"].asString();
	newBuildFile.Url = d["url"].asString();
	newBuildFile.FullPath = buildFolder + to_wstring(newBuild->Id) + L"\\depots\\" + to_wstring(newBuildFile.Name);
	newBuildFile.UnpackedDir = buildFolder + to_wstring(newBuild->Id) + L"\\unpacked\\" + to_wstring(newBuildFile.Name);
	newBuildFile.Downloaded = fs::exists(newBuildFile.FullPath);
	newBuild->Files.push_back(newBuildFile);*/

	fs::path fileName = fs::path(buildFilePath).filename();

	BuildFile* bFile = new BuildFile();
	bFile->Build = build;
	bFile->Name = fileName.string();
	bFile->Url = "";
	bFile->FullPath = buildFilePath;
	bFile->UnpackedDir = UpdateManager::GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(build->App->Host->Uri) + L"\\" + to_wstring(build->App->Id) + L"\\" + to_wstring(build->Id) + L"\\unpacked\\" + to_wstring(GetTickCount()) + L"_" + to_wstring(bFile->Name);
	bFile->Downloaded = true;
	this->createdByFile = true;


	if (bFile->UnpackDepot() == UpdateManager::BuildFile::UnpackResult::Success) {
		this->buildFile = bFile;
		this->windowName = "View [" + this->buildFile->Name + "]";
		parsingFiles = true;
		ParseFiles(this->buildFile->UnpackedDir, &depotFiles);
		parsingFiles = false;
	}
}

ViewWindow::ViewWindow(BuildFile* b) : Window()
{
	this->buildFile = b;
	this->windowName = "View [" + this->buildFile->Name + "]";
	parsingFiles = true;
	ParseFiles(this->buildFile->UnpackedDir, &depotFiles);
	parsingFiles = false;
	selectedChanged = false;
}

void ViewWindow::Close()
{
	this->FreeNodes(&this->depotFiles);
	this->buildFile->UnloadDepot();
	if (this->createdByFile) {
		fs::remove_all(this->buildFile->UnpackedDir);
		delete(this->buildFile);
	}
	Window::Close();
}

void ViewWindow::SetDock(ImGuiID id)
{
	dockId = id;
}

void ViewWindow::RenderFileTree(DirectoryNode* node) {
	if (node == &this->depotFiles)
		ImGui::Indent();
	for (int i = 0; i < node->Children.size(); i++) {
		auto obj = node->Children[i];
		if (obj->IsDirectory) {
			ImGui::Unindent();
			if (ImGui::TreeNode(to_string(obj->FileName).c_str())) {
				ImGui::Indent();
				RenderFileTree(obj);
				ImGui::Unindent();
				ImGui::TreePop();
			}
			ImGui::Indent();
		}
		else {
			if (ImGui::MenuItem(to_string(obj->FileName).c_str(), NULL, selectedFile == node->Children.at(i))) {
				selectedFile = node->Children.at(i);
				selectedChanged = true;
			}
		}
	}
	if (node == &this->depotFiles)
		ImGui::Unindent();
}

void ViewWindow::FreeNodes(DirectoryNode* node)
{
	for (auto& child : node->Children) {
		FreeNodes(child);
	}
	if (node->LoadedFile) {
		if (node->LoadedFile->Image) {
			node->LoadedFile->Image->Release();
			node->LoadedFile->Image = nullptr;
		}
		node->LoadedFile->Text.clear();
		node->LoadedFile->Text.shrink_to_fit();

		node->LoadedFile->BinaryHex.clear();
		node->LoadedFile->BinaryHex.shrink_to_fit();
		delete(node->LoadedFile->Binary);
		delete(node->LoadedFile);
	}
}

bool ViewWindow::Render()
{
	if (!buildFile)
		return false;
	ImGuiID filesDock = ImGui::GetID(("##" + buildFile->Name + "_files").c_str());
	if (this->dockId != -1)
		ImGui::SetNextWindowDockID(this->dockId, ImGuiCond_Once);

	ImGuiWindowClass viewClass;
	viewClass.ClassId = ImGui::GetID(("viewwindow_" + buildFile->Name).c_str());
	viewClass.DockingAllowUnclassed = false;

	if (parsingFiles) // prevent from closing when loading to exclude exception
		ImGui::Begin(("View [" + buildFile->Name + "]").c_str(), nullptr, ImGuiWindowFlags_MenuBar);
	else
		ImGui::Begin(("View [" + buildFile->Name + "]").c_str(), &this->Opened, ImGuiWindowFlags_MenuBar);
	if (parsingFiles) {
		ImGui::ProgressBar(ImGui::GetTime() * -0.2f);
		ImGui::BeginDisabled();
	}

	if (!Window::Render()) {
		ImGui::End();
		return false;
	}

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(this->buildFile->Name.c_str()))
		{
			if (this->buildFile->CheckDepot() != UpdateManager::BuildFile::UnpackResult::Success) // just in case.. :D
				ImGui::BeginDisabled();
			if (ImGui::MenuItem("Open unpacked folder")) {
				OpenFolder(this->buildFile->UnpackedDir);
			}
			if (this->buildFile->CheckDepot() != UpdateManager::BuildFile::UnpackResult::Success)
				ImGui::EndDisabled();
			ImGui::Separator();
			if (!selectedFile)
				ImGui::BeginDisabled();
			if (ImGui::MenuItem("Show selected file")) {
				OpenFolder(this->buildFile->UnpackedDir, selectedFile->FullPath);
			}
			if (!selectedFile)
				ImGui::EndDisabled();

			if (ImGui::MenuItem("Show depot file")) {
				OpenFolder(fs::path(this->buildFile->FullPath).parent_path().wstring(), this->buildFile->FullPath);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}


	ImGui::Columns(2);
	/*ImGui::SetNextItemWidth(-1);
	if (ImGui::BeginListBox("##files")) {

		ImGui::EndListBox();
	}*/

	ImGui::BeginChild("##treeView");
	RenderFileTree(&depotFiles);
	ImGui::EndChild();

	ImGui::NextColumn();
	if (selectedFile && selectedFile->LoadedFile) {
		if (selectedFile->LoadedFile->FileType == LoadedFileType::Encrypted) {
			auto prevCursor = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 75 + ImGui::GetCursorPos().x, ImGui::GetContentRegionAvail().y * 0.5f - 20));
			if (ImGui::Button("Unpack packed file", ImVec2(150, 40))) {
				ViewWindow* packedFile = new ViewWindow(selectedFile->FullPath, this->buildFile->Build);
				packedFile->Show();
			}
			ImGui::SetCursorPos(prevCursor);
		}
		else {
			ImGui::DockSpace(filesDock, ImVec2(0, 0), 0, &viewClass);

			// Text file
			ImGui::SetNextWindowDockID(filesDock, ImGuiCond_Once);
			ImGui::SetNextWindowClass(&viewClass);
			if (selectedFile->LoadedFile->FileType == LoadedFileType::Text && IsSelectedChanged()) {
				ImGui::SetNextWindowFocus();
			}
			ImGui::Begin(("Text##textTab_" + buildFile->Name).c_str());

			ImGui::InputTextMultiline(("##text_" + buildFile->Name).c_str(), (char*)selectedFile->LoadedFile->Text.c_str(), selectedFile->LoadedFile->Text.size(), ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
			ImGui::End();

			// Image file
			ImGui::SetNextWindowDockID(filesDock, ImGuiCond_Once);
			ImGui::SetNextWindowClass(&viewClass);
			if (selectedFile->LoadedFile->FileType == LoadedFileType::Image && IsSelectedChanged()) {
				ImGui::SetNextWindowFocus();
			}
			ImGui::Begin(("Image##image_" + buildFile->Name).c_str());
			if (selectedFile->LoadedFile->Image && DirectX::LoadedImages.find(selectedFile->LoadedFile->Image) != DirectX::LoadedImages.end()) {
				DirectX::LoadedImage info = DirectX::LoadedImages[selectedFile->LoadedFile->Image]; // there always will be info about image
				float aspect = (float)info.Width / (float)max(info.Height, 1);
				ImGui::Image(selectedFile->LoadedFile->Image, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x / aspect));
			}
			ImGui::End();

			// Binary file
			ImGui::SetNextWindowDockID(filesDock, ImGuiCond_Once);
			ImGui::SetNextWindowClass(&viewClass);
			if (selectedFile->LoadedFile->FileType == LoadedFileType::Binary && IsSelectedChanged()) {
				ImGui::SetNextWindowFocus();
			}
			ImGui::Begin(("Binary##binary_" + buildFile->Name).c_str());
			ImGui::InputTextMultiline(("##binary_" + buildFile->Name).c_str(), (char*)selectedFile->LoadedFile->BinaryHex.c_str(), selectedFile->LoadedFile->BinaryHex.size(), ImVec2(-1, ImGui::GetContentRegionAvail().y - 30), ImGuiInputTextFlags_ReadOnly);
			if (ImGui::InputInt("Current row", &this->currentBinaryRow)) {
				if (this->currentBinaryRow < 0)
					this->currentBinaryRow = 0;

				ImGuiContext& g = *GImGui;
				const char* child_window_name = NULL;
				ImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g.CurrentWindow->Name, ("##binary_" + buildFile->Name).c_str(), ImGui::GetID(("##binary_" + buildFile->Name).c_str()));
				ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);
				ImGui::SetScrollY(child_window, this->currentBinaryRow * ImGui::CalcTextSize("a").y + 3);

			}

			ImGui::End();
		}
	}
	ImGui::Columns(1);

	//ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	//ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	//if (ImGui::BeginPopupModal("File is missing")) {

	//	ImGui::EndPopup();
	//}
	if (parsingFiles)
		ImGui::EndDisabled();
	ImGui::End();
	return true;
}

BuildFile* ViewWindow::GetBuildFile()
{
	return this->buildFile;
}
