#include "UpdateManager.h"
#include "Utils/httplib/httplib.h"
#include "Utils/jsoncpp/json/json.h"

string UpdateManager::BuildFile::DFileTypeToString(BuildFile::DFileType type)
{
	switch (type)
	{
	case BuildFile::DFileType::Default:
		return "Default";
	case BuildFile::DFileType::Encrypted:
		return "Encrypted";
	case BuildFile::DFileType::EncryptedFile:
		return "EncryptedFile";
	case BuildFile::DFileType::Key:
		return "Key";
	}
	return "Unknown";
}

std::vector<Host>* UpdateManager::GetHosts(bool enforce)
{
	if (!enforce && Hosts.size())
		return &Hosts;
	std::vector<Host> ret = std::vector<Host>();
	if (fs::exists(GetExecutableFolder().wstring() + L"\\updates\\"))
	{
		for (auto obj : fs::directory_iterator(GetExecutableFolder().wstring() + L"\\updates\\"))
		{
			Host newHost;
			newHost.IsAdmin = false;
			newHost.Uri = obj.path().filename().string();
			ret.push_back(newHost);
		}
		Hosts = ret;
	}
	return &Hosts;
}

fs::path UpdateManager::GetExecutableFolder()
{
	if (executableFolder != nullopt)
		return executableFolder.value();
	wchar_t tPath[255];
	if (GetModuleFileName(GetModuleHandle(NULL), (LPWSTR)&tPath, 255) != 0) {
		wstring strPath = wstring(tPath);
		fs::path path = fs::path(strPath).parent_path();
		LogW(L"GetModuleFileName: " + path.wstring());
		executableFolder = path;
		return path;
	}
	return fs::path();
}

vector<App>* UpdateManager::Host::GetVersions(bool enforce)
{
	if (!enforce && this->Apps.size())
		return &this->Apps;
	std::vector<App> ret = std::vector<App>();
	if (fs::exists(GetExecutableFolder().wstring() + L"\\updates\\"))
	{
		for (auto obj : fs::directory_iterator(GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->Uri) + L"\\"))
		{
			App newVersion;
			newVersion.Id = obj.path().filename().string();
			newVersion.Host = this;
			ret.push_back(newVersion);
		}
		this->Apps = ret;
	}
	return &this->Apps;
}

std::optional<Json::Value> GetDetailsJSON(string host, string ver, string ghub) {
	httplib::Client cli("https://" + host);
	auto res = cli.Get("/pipeline/" + ver + "/update/" + ghub + "/win/public/details.json");
	if (res->status != 200)
		return nullopt;
	Json::Value root;
	Json::Reader reader;
	reader.parse(res->body, root);
	return root;
}

std::optional<Json::Value> GetDetailsJSON(string json) {
	/*if (res->status != 200)
		return nullopt;*/
	Json::Value root;
	Json::Reader reader;
	reader.parse(json, root);
	return root;
}

vector<Build>* UpdateManager::App::GetBuilds(bool enforce)
{
	const auto buildFolder = GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->Host->Uri) + L"\\" + to_wstring(this->Id) + L"\\";

	if (!enforce && this->Builds.size())
		return &this->Builds;
	Log("Getting builds");

	string lastBuildId = "";

	auto lastBuild = GetDetailsJSON(this->Host->Uri, "v2", this->Id);
	if (lastBuild.has_value()) {
		lastBuildId = lastBuild.value()["buildId"].asString();
		auto buildDirectory = buildFolder + to_wstring(lastBuildId);
		if (!fs::exists(buildDirectory))
			fs::create_directories(buildDirectory);

		WriteToFile(buildDirectory + L"\\details.json", lastBuild.value().toStyledString());
		Log("Found last build on the server: " + lastBuildId);
	}

	std::vector<Build> ret = std::vector<Build>();
	if (fs::exists(GetExecutableFolder().wstring() + L"\\updates\\"))
	{
		for (auto obj : fs::directory_iterator(buildFolder))
		{
			Build* newBuild = new Build(); // idk why but if we do regular object without new, it will be free after this function (btw, todo: fix memory leaks :D)
			newBuild->App = this;
			newBuild->Id = obj.path().filename().string();
			newBuild->LastBuild = lastBuildId == obj.path().filename().string();

			std::optional<Json::Value> details;

			if (newBuild->LastBuild)
			{
				details = lastBuild;
			}
			else
				if (this->Host->IsAdmin) {

				}

			if (!details.has_value()) {
				auto buildDetailsDir = buildFolder + to_wstring(newBuild->Id) + L"\\details.json";
				if (fs::exists(buildDetailsDir))
				{
					details = GetDetailsJSON(ReadFromFile(buildDetailsDir));
					Log("Found local \"details.json\" for build: " + details.value()["buildId"].asString());
				}
			}

			if (details.has_value()) {
				for (unsigned int i = 0; i < details.value()["depots"].size(); i++) {
					auto d = details.value()["depots"][i];
					BuildFile newBuildFile;
					newBuildFile.Build = newBuild;
					newBuildFile.Name = d["name"].asString();
					newBuildFile.Url = d["url"].asString();
					newBuildFile.FullPath = buildFolder + to_wstring(newBuild->Id) + L"\\depots\\" + to_wstring(newBuildFile.Name);
					newBuildFile.UnpackedDir = buildFolder + to_wstring(newBuild->Id) + L"\\unpacked\\" + to_wstring(newBuildFile.Name);
					newBuildFile.Downloaded = fs::exists(newBuildFile.FullPath);
					newBuild->Files.push_back(newBuildFile);
					//break;
				}
			}

			ret.push_back(*newBuild);
		}
		this->Builds = ret;
	}
	return &this->Builds;
}

vector<BuildFile>* UpdateManager::Build::GetFiles()
{
	return &this->Files;
}

void UpdateManager::BuildFile::DownloadDepot()
{
	Log("Downloading file " + this->Name);

	httplib::Client cli("https://" + this->Build->App->Host->Uri);
	auto res = cli.Get(this->Url);
	fs::create_directories(fs::path(this->FullPath).parent_path());
	WriteToFile(this->FullPath, res->body.data(), res->body.size());
	this->Downloaded = true;
}

BuildFile::LoadResult UpdateManager::BuildFile::LoadDepot(bool force)
{
	if (!force && this->lastLoadResult != LoadResult::Null)
		return this->lastLoadResult;
	if (this->lastLoadResult == LoadResult::Success)
		return LoadResult::Success;
	if (!fs::exists(this->FullPath)) {
		Log("Failed to load file " + this->Name + ", Error: FileNotFound");
		this->lastLoadResult = LoadResult::FileNotFound;
		return LoadResult::FileNotFound;
	}

	//if (fs::exists(this->UnpackedDir)) {
	//	return BuildFile::LoadResult::Success;
	//}

	size_t depotSize = fs::file_size(this->FullPath);
	this->Depot = (char*)malloc(depotSize);
	this->DepotSize = depotSize;
	if (this->Depot == nullptr)
	{
		//free(this->Depot);
		//this->Depot = nullptr;
		Log("Failed to load file " + this->Name + ", Error: UnknownError (malloc failed)");
		this->lastLoadResult = LoadResult::UnknownError;
		return LoadResult::UnknownError;
	}
	memset(this->Depot, 0, depotSize);
	ReadBinaryFile(this->FullPath, this->Depot, depotSize);

	this->FileType = *(BuildFile::DFileType*)this->Depot;
	{
		int validEnum = (int)BuildFile::DFileType::Default | (int)BuildFile::DFileType::Encrypted | (int)BuildFile::DFileType::EncryptedFile | (int)BuildFile::DFileType::Key;
		if (!((int)this->FileType & validEnum))
		{
			free(this->Depot);
			this->Depot = nullptr;
			Log("Failed to load file " + this->Name + ", Error: UnknownFileType");
			this->lastLoadResult = LoadResult::UnknownFileType;
			return LoadResult::UnknownFileType;
		}
	}
	Log("Loaded file: " + this->Name + ", Size: " + to_string(depotSize) + " bytes, FileType: " + DFileTypeToString(this->FileType));
	this->lastLoadResult = LoadResult::Success;
	return LoadResult::Success;
}

void UpdateManager::BuildFile::UnloadDepot()
{
	if (this->Depot != nullptr)
		free(this->Depot);
	this->Depot = nullptr;
	this->DepotSize = 0;
	this->lastLoadResult = BuildFile::LoadResult::Null;
}

BuildFile::UnpackResult UpdateManager::BuildFile::CheckDepot(bool force)
{
	if (!force && this->lastCheckResult != UnpackResult::Null)
		return this->lastCheckResult;
	if (this->lastCheckResult == UnpackResult::Success)
		return UnpackResult::Success;
	if (this->Depot == nullptr)
	{
		BuildFile::LoadResult loadResult = BuildFile::LoadDepot();
		if (loadResult != BuildFile::LoadResult::Success)
		{
			this->lastCheckResult = BuildFile::UnpackResult::LoadError;
			return BuildFile::UnpackResult::LoadError;
		}
	}

	if (fs::exists(this->UnpackedDir))
	{
		this->lastCheckResult = BuildFile::UnpackResult::Success;
		return BuildFile::UnpackResult::Success;
	}
	this->lastCheckResult = BuildFile::UnpackResult::NotUnpackedYet;
	return BuildFile::UnpackResult::NotUnpackedYet;
}

BuildFile::UnpackResult UpdateManager::BuildFile::UnpackDepot(int* progress, int* progressMax, bool force)
{
	if (!force && this->lastUnpackResult != UnpackResult::Null)
		return this->lastUnpackResult;

	if (this->LoadDepot() != LoadResult::Success)
	{
		this->lastUnpackResult = UnpackResult::LoadError;
		return UnpackResult::LoadError;
	}

	UnpackResult CheckResult = this->CheckDepot();

	if (CheckResult != UnpackResult::Success && CheckResult != UnpackResult::NotUnpackedYet) {
		this->lastUnpackResult = this->lastCheckResult;
		return this->lastUnpackResult;
	}

	Json::Value JSONData;
	unsigned int JSONLength = *(unsigned int*)(this->Depot + sizeof(BuildFile::DFileType));
	{
		Json::Reader reader;

		string json = string(this->Depot + sizeof(BuildFile::DFileType) + sizeof(unsigned int), this->Depot + sizeof(BuildFile::DFileType) + sizeof(unsigned int) + JSONLength);
		reader.parse(json, JSONData);
	}

	// Reading files
	int offset = sizeof(BuildFile::DFileType) + sizeof(unsigned int) + JSONLength;
	if (progress != nullptr)
		*progress = 0;
	if (progressMax)
		*progressMax = JSONData["files"].size();
	for (unsigned int i = 0; i < JSONData["files"].size(); i++) {
		string fileName = JSONData["files"][i]["name"].asString();
		//Log("Unpacking file " + fileName);
		wstring filePathStr = this->UnpackedDir + L"\\" + to_wstring(fileName);
		std::replace(filePathStr.begin(), filePathStr.end(), L'/', L'\\');
		fs::path filePath = fs::path(filePathStr).parent_path();


		unsigned int fileSize = *(unsigned int*)(this->Depot + offset);
		offset += sizeof(unsigned int);
		fs::create_directories(filePath);
		WriteToFile(this->UnpackedDir + L"\\" + to_wstring(fileName), this->Depot + offset, fileSize);
		offset += fileSize;
		Log("Unpacked file " + fileName + ", Size: " + to_string(fileSize));
		if (progress != nullptr)
			(*progress)++;
	}

	this->lastUnpackResult = UnpackResult::Success;
	this->CheckDepot();
	if (progress != nullptr)
		*progress = 0;
	if (progressMax != nullptr)
		*progressMax = 0;
	return UnpackResult::Success;
}

BuildFile::UnpackResult UpdateManager::BuildFile::UnpackDepot(bool force)
{
	return UnpackDepot(nullptr, nullptr, force);
}
