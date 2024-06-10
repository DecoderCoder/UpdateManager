#include "ViewWindow.h"
#include "../../DirectX/D3DX11tex.h"
#include "../../Global.h"

void CopyFilesRecursive(wstring to, wstring from, wstring parentFolder) {
	if (fs::is_directory(from))
	{
		fs::create_directories(to + L"\\" + fs::relative(from, parentFolder).wstring());
		for (auto obj : fs::directory_iterator(from)) {
			CopyFilesRecursive(to, obj.path().wstring(), parentFolder);
		}
	}
	fs::copy(from, to + L"\\" + fs::relative(from, parentFolder).wstring());
}

void ViewWindow::DropCallback(std::vector<std::wstring> files)
{
	if (files.size() == 0)
		return;
	if (parsingFiles)
		return;

	std::wstring parentFolder = fs::path(files[0]).parent_path();
	for (auto obj : files)
	{
		CopyFilesRecursive(this->buildFile->UnpackedDir, obj, parentFolder);
	}

	RefreshFiles();
}

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
		else
		{
			this->FilesCount++;
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
			else if (ext == ".webp")
				loadedFile->FileType = LoadedFileType::Image;
			else
				loadedFile->FileType = LoadedFileType::Binary;


			ReadBinaryFile(loadedFile->FullPath, &loadedFile->Binary, loadedFile->BinarySize);
			this->FilesSize += loadedFile->BinarySize;

			if (*(BuildDepot::DFileType*)loadedFile->Binary == BuildDepot::DFileType::Encrypted || *(BuildDepot::DFileType*)loadedFile->Binary == BuildDepot::DFileType::EncryptedFile)
				loadedFile->FileType = LoadedFileType::Encrypted;
			//if (loadedFile->BinarySize < 1 * 1024 * 1024) {
			if (!Settings::LowRAM) {
				loadedFile->Text = string(loadedFile->Binary, loadedFile->BinarySize);
				loadedFile->BinaryHex = ToHex2(loadedFile->Binary, loadedFile->BinarySize, true);
				DirectX::LoadTextureFromFile(loadedFile->FullPath, loadedFile->Image);
			}

			//}


			//if (loadedFile->FileType == LoadedFileType::Image) {

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

	BuildDepot* bFile = new BuildDepot();
	bFile->Build = build;
	bFile->Name = fileName.string();
	bFile->Url = "";
	bFile->FullPath = buildFilePath;
	bFile->UnpackedDir = UpdateManager::GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(build->App->Host->Uri) + L"\\" + to_wstring(build->App->Id) + L"\\" + to_wstring(build->Id) + L"\\unpacked\\" + to_wstring(GetTickCount()) + L"_" + to_wstring(bFile->Name);
	bFile->Downloaded = true;
	this->createdByFile = true;


	if (bFile->UnpackDepot() == UpdateManager::BuildDepot::UnpackResult::Success) {
		auto before = std::chrono::system_clock::now();
		this->buildFile = bFile;
		this->windowName = "View [" + this->buildFile->Name + "]";
		parsingFiles = true;
		ParseFiles(this->buildFile->UnpackedDir, &depotFiles);
		parsingFiles = false;
		Log("Depot opened in " + to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - before).count()) + " milliseconds");
	}
}

ViewWindow::ViewWindow(BuildDepot* b) : Window()
{
	this->buildFile = b;
	this->windowName = "View [" + this->buildFile->Name + "]";
	parsingFiles = true;
	auto before = std::chrono::system_clock::now();
	ParseFiles(this->buildFile->UnpackedDir, &depotFiles);
	Log("File opened in " + to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - before).count()) + " milliseconds");
	parsingFiles = false;
	selectedChanged = false;
}

void ViewWindow::Close()
{
	if (this->buildFile->Build->App->Host->IsAdmin && this->hwnd)
		RevokeDragDrop(this->hwnd);
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
			string size = Utils::SizeToString(obj->LoadedFile->BinarySize);
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(size.c_str()).x - 20);
			ImGui::TextDisabled(size.c_str());
		}
	}
	if (node == &this->depotFiles)
		ImGui::Unindent();

}

void ViewWindow::RefreshFiles()
{
	this->parsingFiles = true;
	selectedFile = nullptr;
	this->parsingFilesFuture = std::async(std::launch::async, [&]() {
		this->FilesCount = 0;
		this->FreeNodes(&this->depotFiles);
		this->ParseFiles(this->buildFile->UnpackedDir, &depotFiles);
		this->parsingFiles = false;
		});
}

void ViewWindow::FreeNodes(DirectoryNode* node)
{
	for (auto& child : node->Children) {
		FreeNodes(child);
	}
	node->Children.clear();
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
		node->LoadedFile->Binary = nullptr;
		node->LoadedFile = nullptr;
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

	bool disabled = parsingFiles;

	bool disabledStarted = false; // to prevent stack error when Parsing finish earlier than render

	ImGui::SetNextWindowSize(ImVec2(600, 800), ImGuiCond_FirstUseEver);
	if (parsingFiles) // prevent from closing when loading to exclude exception
		ImGui::Begin(("View [" + buildFile->Name + "]").c_str(), nullptr, ImGuiWindowFlags_MenuBar);
	else
		ImGui::Begin(("View [" + buildFile->Name + "]").c_str(), &this->Opened, ImGuiWindowFlags_MenuBar);
	if (disabled) {
		ImGui::ProgressBar(ImGui::GetTime() * -0.2f);
		ImGui::BeginDisabled();
		disabledStarted = true;
	}

	if (this->buildFile->Build->App->Host->IsAdmin && !hwnd && ImGui::GetWindowViewport()->PlatformHandle)
	{
		hwnd = (HWND)ImGui::GetWindowViewport()->PlatformHandle;
		this->dropManager.dropCallback = [&](std::vector<std::wstring> files) {
			DropCallback(files);
			};
		RegisterDragDrop(hwnd, &this->dropManager);
	}
	if (!Window::Render()) {
		ImGui::End();
		return false;
	}

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(this->buildFile->Name.c_str()))
		{
			bool unpackedDirExists = fs::exists(this->buildFile->UnpackedDir);
			bool depotExists = fs::exists(this->buildFile->FullPath);
			if (!unpackedDirExists)
				ImGui::BeginDisabled();
			if (ImGui::MenuItem("Open unpacked folder")) {
				OpenFolder(this->buildFile->UnpackedDir);
			}
			if (!unpackedDirExists)
				ImGui::EndDisabled();
			ImGui::Separator();
			if (!selectedFile)
				ImGui::BeginDisabled();
			if (ImGui::MenuItem("Show selected file")) {
				OpenFolder(this->buildFile->UnpackedDir, selectedFile->FullPath);
			}
			if (!selectedFile)
				ImGui::EndDisabled();

			if (!depotExists)
				ImGui::BeginDisabled();
			if (ImGui::MenuItem("Show depot file")) {
				OpenFolder(fs::path(this->buildFile->FullPath).parent_path().wstring(), this->buildFile->FullPath);
			}
			if (!depotExists)
				ImGui::EndDisabled();
			ImGui::EndMenu();

		}
		ImGui::EndMenuBar();
	}

	ImGui::Columns(2, 0, false);
	ImGui::SetColumnWidth(0, 315);
	if (!this->buildFile->Build->App->Host->IsAdmin)
		ImGui::BeginDisabled();
	ImGui::BeginChild("##depotinfo", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border);

	ImGui::Text(("Depot name  : " + this->buildFile->Name).c_str());

	if (this->buildFile->DepotSize == 0)
		ImGui::Text(("Files size  : " + Utils::SizeToString(this->FilesSize)).c_str());
	else
		ImGui::Text(("Depot size  : " + Utils::SizeToString(this->buildFile->DepotSize)).c_str());
	ImGui::Text(("Files count : " + to_string(this->FilesCount)).c_str());

	ImGui::Text(("File Type   : " + string(this->buildFile->Key.IsValid() ? "Encrypted" : "Default")).c_str());

	ImGui::Text("Key");


	ImGui::SetNextItemWidth(ImGui::CalcTextSize("53836da6-de2f-44b8-8454-1f0ccb4b1e65").x + ImGui::GetStyle().FramePadding.x * ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::BeginCombo("##selectfiletype", this->buildFile->Key.Name.c_str())) {
		auto app = this->buildFile->Build->App;

		if (ImGui::Selectable("##emptykey")) {
			this->buildFile->Key = UpdateManager::KeyManager::EmptyKey;
		}

		for (int i = 0; i < app->AccessGroup->Keys.size(); i++) {
			if (ImGui::Selectable(app->AccessGroup->Keys[i].Name.c_str())) {
				this->buildFile->Key = app->AccessGroup->Keys[i];
			}
		}

		ImGui::EndCombo();
	}

	ImGui::EndChild();

	ImGui::BeginChild("##filesexplorer", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border);

	ImGui::TextDisabled("You can drop files on this window");
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 15.f / 2.f);

	//auto buttonPos = ImGui::GetCursorPos();
	//buttonPos.x += ImGui::GetWindowPos().x;
	//buttonPos.y += ImGui::GetWindowPos().y;

	//auto buttonCenter = ImVec2(buttonPos.x + 15.f / 2.f, buttonPos.y + 15.f / 2.f);

	if (disabled)
		ImGui::BeginDisabled();
	if (ImGui::Button("R##refreshbuttom", ImVec2(15, 15))) {
		RefreshFiles();
	}
	if (disabled)
		ImGui::EndDisabled();
	ImGui::SetItemTooltip("Update file tree");
	{
		//auto color = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);
		//auto draw = ImGui::GetWindowDrawList();
		//draw->AddCircle(ImVec2(buttonCenter.x, buttonCenter.y), 5, color);
	}
	HANDLE handle = ImGui::GetWindowViewport()->PlatformHandle;

	ImGui::EndChild();
	if (ImGui::BeginDragDropTarget()) {
		if (ImGui::AcceptDragDropPayload("FILES"))  // or: const ImGuiPayload* payload = ... if you sent a payload in the block above
		{
			// draggedFiles is my vector of strings, how you handle your payload is up to you
			Log("DROPED FILES");
		}
		ImGui::EndDragDropTarget();
	}

	if (!this->buildFile->Build->App->Host->IsAdmin)
		ImGui::EndDisabled();

	ImGui::BeginChild("##treeView", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border);
	RenderFileTree(&depotFiles);
	ImGui::EndChild();




	/*ImGui::SetNextItemWidth(-1);
	if (ImGui::BeginListBox("##files")) {

		ImGui::EndListBox();
	}*/


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
			if (selectedFile && this->selectedFile->LoadedFile->Text == "")
				this->selectedFile->LoadedFile->Text = string(this->selectedFile->LoadedFile->Binary, this->selectedFile->LoadedFile->BinarySize);
			ImGui::InputTextMultiline(("##text_" + buildFile->Name).c_str(), (char*)selectedFile->LoadedFile->Text.c_str(), selectedFile->LoadedFile->Text.size(), ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
			ImGui::End();

			// Image file
			ImGui::SetNextWindowDockID(filesDock, ImGuiCond_Once);
			ImGui::SetNextWindowClass(&viewClass);
			if (selectedFile->LoadedFile->FileType == LoadedFileType::Image && IsSelectedChanged()) {
				ImGui::SetNextWindowFocus();
			}
			ImGui::Begin(("Image##image_" + buildFile->Name).c_str());
			if (selectedFile->LoadedFile->Image == nullptr)
				DirectX::LoadTextureFromFile(selectedFile->LoadedFile->FullPath, selectedFile->LoadedFile->Image);

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
			if (selectedFile && this->selectedFile->LoadedFile->BinaryHex == "")
				this->selectedFile->LoadedFile->BinaryHex = ToHex2(this->selectedFile->LoadedFile->Binary, this->selectedFile->LoadedFile->BinarySize, true);
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
	if (disabled || disabledStarted)
		ImGui::EndDisabled();
	ImGui::End();
	return true;
}

BuildDepot* ViewWindow::GetBuildDepot()
{
	return this->buildFile;
}
