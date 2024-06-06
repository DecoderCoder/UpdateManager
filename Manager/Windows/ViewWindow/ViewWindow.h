#pragma once
#include "../../DirectX/DirectX.h"
#include "../../UpdateManager/UpdateManager.h"

class ViewWindow : public Window {
	enum class LoadedFileType {
		Image,
		Text,
		Binary,
		Encrypted
	};

	struct LoadedFile {
		std::wstring FullPath;
		LoadedFileType FileType = LoadedFileType::Binary;
		ID3D11ShaderResourceView* Image = nullptr;
		char* Binary;
		size_t BinarySize;
		string Text = "";
		string BinaryHex = "";
	};

	struct DirectoryNode
	{
		std::wstring FullPath;
		std::wstring FileName;
		std::vector<DirectoryNode*> Children;
		LoadedFile* LoadedFile = nullptr;
		bool IsDirectory = false;
	};

	int currentBinaryRow = 0;
	DirectoryNode depotFiles;
	DirectoryNode* selectedFile;
	bool showed = false;
	bool selectedChanged = false;
	bool parsingFiles = false;
	bool IsSelectedChanged();
	void ParseFiles(wstring path, DirectoryNode* parentNode);
	void RenderFileTree(DirectoryNode* node);

	void FreeNodes(DirectoryNode* node);

	BuildDepot* buildFile = nullptr;
	ImGuiID dockId = -1;
	bool createdByFile = false;
public:
	ViewWindow(wstring buildFilePath, UpdateManager::Build* build);
	ViewWindow(BuildDepot* b);
	virtual void Close();
	virtual void SetDock(ImGuiID id);
	virtual bool Render();
	BuildDepot* GetBuildDepot();
};