#include "ViewWindow.h"
#include "../../DirectX/D3DX11tex.h"

bool ViewWindow::IsSelectedChanged() {	// to prevent code dup 
	bool temp = selectedChanged;
	if (selectedChanged)
		selectedChanged = false;
	return temp;
}

void ViewWindow::ParseFiles(wstring path, DirectoryNode* parentNode) {
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

			loadedFile->Text = ReadFromFile(loadedFile->FullPath);

			{
				loadedFile->Binary = ToHex(loadedFile->Text.data(), loadedFile->Text.size(), false);

				//	size_t size = 0;
				//	char* buff = nullptr;
				//	ReadBinaryFile(loadedFile->FullPath, &buff, size);
				//	loadedFile->Text = string(buff, size);
			}

			if (loadedFile->FileType == LoadedFileType::Image) {
				DirectX::LoadTextureFromFile(loadedFile->FullPath, loadedFile->Image);
			}

			newNode->LoadedFile = loadedFile;

			parentNode->Children.push_back(newNode);
			if (!selectedFile)
				selectedFile = parentNode->Children.at(parentNode->Children.size() - 1);
		}

	}
}

ViewWindow::ViewWindow(BuildFile* b) : Window()
{
	this->buildFile = b;
	this->windowName = "View [" + this->buildFile->Name + "]";
	parsingFiles = true;
	ParseFiles(this->buildFile->UnpackedDir, &depotFiles);
	parsingFiles = false;
}

void ViewWindow::Close()
{
	this->FreeNodes(&this->depotFiles);
	this->buildFile->UnloadDepot();
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

		node->LoadedFile->Binary.clear();
		node->LoadedFile->Binary.shrink_to_fit();
		delete(node->LoadedFile);
	}
}

bool ViewWindow::Render()
{
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

	RenderFileTree(&depotFiles);

	ImGui::NextColumn();
	if (selectedFile && selectedFile->LoadedFile) {
		ImGui::DockSpace(filesDock, ImVec2(0, 0), 0, &viewClass);

		ImGui::SetNextWindowDockID(filesDock, ImGuiCond_Once);
		ImGui::SetNextWindowClass(&viewClass);
		if (selectedFile->LoadedFile->FileType == LoadedFileType::Text && IsSelectedChanged()) {
			ImGui::SetNextWindowFocus();
		}
		ImGui::Begin(("Text##textTab_" + buildFile->Name).c_str());

		ImGui::InputTextMultiline(("##text_" + buildFile->Name).c_str(), (char*)selectedFile->LoadedFile->Text.c_str(), selectedFile->LoadedFile->Text.size(), ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
		ImGui::End();



		ImGui::SetNextWindowDockID(filesDock, ImGuiCond_Once);
		ImGui::SetNextWindowClass(&viewClass);
		if (selectedFile->LoadedFile->FileType == LoadedFileType::Image && IsSelectedChanged()) {
			ImGui::SetNextWindowFocus();
		}
		ImGui::Begin(("Image##image_" + buildFile->Name).c_str());
		ImGui::Image(selectedFile->LoadedFile->Image, ImVec2(100, 100));
		ImGui::End();

		ImGui::SetNextWindowDockID(filesDock, ImGuiCond_Once);
		ImGui::SetNextWindowClass(&viewClass);
		if (selectedFile->LoadedFile->FileType == LoadedFileType::Binary && IsSelectedChanged()) {
			ImGui::SetNextWindowFocus();
		}
		ImGui::Begin(("Binary##binary_" + buildFile->Name).c_str());
		ImGui::InputTextMultiline(("##binary_" + buildFile->Name).c_str(), (char*)selectedFile->LoadedFile->Binary.c_str(), selectedFile->LoadedFile->Binary.size(), ImVec2(-1, ImGui::GetContentRegionAvail().y - 30), ImGuiInputTextFlags_ReadOnly);
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
