#include "UpdateManager.h"

#include "Utils/httplib/httplib.h"

std::map<Host*, std::future<void>> fGetAccessGroups;
std::map<Host*, std::future<void>> fGetApps;
std::map<App*, std::future<void>> fGetBuilds;

string UpdateManager::BuildDepot::DFileTypeToString(BuildDepot::DFileType type)
{
	switch (type)
	{
	case BuildDepot::DFileType::Default:
		return "Default";
	case BuildDepot::DFileType::Encrypted:
		return "Encrypted";
	case BuildDepot::DFileType::EncryptedFile:
		return "EncryptedFile";
		/*case BuildFile::DFileType::Key:
			return "Key";*/
	}
	return "Unknown";
}

void UpdateManager::AddHost(string host, bool isAdmin, string login, string password) {
	//for (auto obj : this->) {
	//	if (obj.Uri == host)
	//		return;
	//}

	wstring dir = UpdateManager::GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(host);

	fs::create_directories(dir);
	inih::INIReader r;
	r.InsertEntry<bool>("host", "is_admin", isAdmin);
	r.InsertEntry<string>("host", "login", login);
	r.InsertEntry<string>("host", "password", password);

	inih::INIWriter::write(to_string((dir + L"\\settings.ini")), r);
	GetHosts(true);
}

void UpdateManager::RemoveHost(string name)
{
	if (fs::exists(UpdateManager::GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(name)))
		fs::remove_all(UpdateManager::GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(name));
	for (int i = 0; i < UpdateManager::Hosts.size(); i++) {
		if (name == UpdateManager::Hosts[i].Uri) {
			UpdateManager::Hosts.erase(UpdateManager::Hosts.begin() + i);
		}
	}
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

			if (fs::exists(obj.path().wstring() + L"\\settings.ini")) {
				inih::INIReader r{ to_string((obj.path().wstring() + L"\\settings.ini")) };
				newHost.IsAdmin = r.Get<bool>("host", "is_admin", false);
				newHost.Login = r.Get<string>("host", "login", "");
				newHost.Password = r.Get<string>("host", "password", "");
			}
			ret.emplace_back(newHost);
		}
		Hosts = ret;
	}
	for (int i = 0; i < Hosts.size(); i++) {
		Host* host = &Hosts.at(i);
		if (host->IsAdmin) {
			fGetAccessGroups[host] = std::async(std::launch::async, [](Host* host) {
				httplib::Client cli("https://" + host->Uri);
				auto res = cli.Get("/pipeline/v2/update/access_groups.json");
				Json::Value root;
				Json::Reader reader;
				reader.parse(res->body, root);
				for (auto obj : root["accessGroups"]) {
					auto accessGroup = host->AddAccessGroup(obj["name"].asString(), obj["value"].asString());
					if (accessGroup)
						for (auto key : obj["keys"]) {
							UpdateManager::KeyManager::Key nKey;
							nKey.Name = key["name"].asString();
							nKey.Value = key["value"].asString();
							accessGroup->Keys.push_back(nKey);
						}
				}
				}, host);
		}
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

// -1 Do nothing
//  0 Restore
//  1 Create new

UpdateManager::Host::AccessGroup* UpdateManager::Host::GetAccessGroup(string value)
{
	for (int i = 0; i < this->accessGroups.size(); i++) {
		if (this->accessGroups[i]->Value == value)
			return this->accessGroups.at(i);;
	}
	return nullptr;
}

bool UpdateManager::Host::HasAccessGroupName(string name)
{
	for (auto obj : this->accessGroups) {
		if (obj->Name == name)
			return true;
	}
	return false;
}

bool UpdateManager::Host::HasAccessGroup(string value)
{
	for (auto obj : this->accessGroups) {
		if (obj->Value == value)
			return true;
	}
	return false;
}

UpdateManager::Host::AccessGroup* UpdateManager::Host::AddAccessGroup(string name, string value, bool online, AddAccessGroupResponse* result)
{
	if (this->IsAdmin && online) {
		Json::Value root;
		Json::Reader reader;
		httplib::Client cli("https://" + this->Uri);
		string auth = this->Login + ":" + this->Password;
		httplib::Headers headers = {
  { "Authorization",  base64_encode((const BYTE*)auth.data(), auth.size())}
		};
		httplib::Result res = cli.Get("/pipeline/v2/update/access_group/add/" + name + "/" + value, headers);
		reader.parse(res->body, root);
		if (root["status"].asString() != "ok") {
			if (result != nullptr) {
				if (root["status"].asString() == "already_exists") {
					*result = AddAccessGroupResponse::AlreadyExists;
				}
			}
			return nullptr;
		}
	}
	UpdateManager::Host::AccessGroup* group = new UpdateManager::Host::AccessGroup(this);
	group->Name = name;
	group->Value = value;
	this->accessGroups.push_back(group);
	if (result != nullptr) {
		*result = AddAccessGroupResponse::Success;
	}
	return group;
}

UpdateManager::Host::AddAppResponse UpdateManager::Host::AddApp(string name, string accessGroupValue, int ifExists)
{
	if (this->IsAdmin) {
		if (accessGroupValue == "")
			accessGroupValue = "null";
		Json::Value root;
		Json::Reader reader;
		httplib::Client cli("https://" + this->Uri);
		string auth = this->Login + ":" + this->Password;
		httplib::Headers headers = {
  { "Authorization",  base64_encode((const BYTE*)auth.data(), auth.size())}
		};
		httplib::Result res;
		if (ifExists == 0)
			res = cli.Get("/pipeline/v2/update/app/add/" + name + "/" + accessGroupValue + "/restore", headers);
		else if (ifExists == 1)
			res = cli.Get("/pipeline/v2/update/app/add/" + name + "/" + accessGroupValue + "/create", headers);
		else
			res = cli.Get("/pipeline/v2/update/app/add/" + name + "/" + accessGroupValue, headers);
		reader.parse(res->body, root);
		if (!root["status"].isNull()) {

			string status = root["status"].asString();
			if (status == "has_deleted") {
				return Host::AddAppResponse::HasDeleted;
			}
			else if (status == "already_exists") {
				return Host::AddAppResponse::AlreadyExists;
			}
		}
		return Host::AddAppResponse::Success;
	}
	else {
		auto path = GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->Uri) + L"\\" + to_wstring(name);
		fs::create_directories(path);
	}
	this->GetApps(true);
	return Host::AddAppResponse::Success;
}

void UpdateManager::Host::RemoveApp(string name)
{
	int appIndex = 0;
	App* app = nullptr;
	for (int i = 0; i < this->Apps.size(); i++) {
		if (this->Apps[i].Id == name) {
			appIndex = i;
			app = &this->Apps[appIndex];
			break;
		}
	}
	if (!app)
		return;

	auto path = GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->Uri) + L"\\" + to_wstring(app->Id);
	if (fs::exists(path))
		fs::remove_all(path);

	if (app->OnServer) { // OnServer sets only when admin
		httplib::Client cli("https://" + this->Uri);
		string auth = this->Login + ":" + this->Password;
		httplib::Headers headers = {
  { "Authorization",  base64_encode((const BYTE*)auth.data(), auth.size())}
		};
		auto res = cli.Get("/pipeline/v2/update/app/remove/" + name, headers);
	}

	this->Apps.erase(this->Apps.begin() + appIndex);
}

vector<App>* UpdateManager::Host::GetApps(bool enforce)
{
	if (!enforce && this->Apps.size()) {
		return &this->Apps;
	}
	if (!this->WaitingGetApps)
	{
		this->WaitingGetApps = true;
		fGetApps[this] = std::async(std::launch::async, [&]()
			{

				std::vector<App> ret = std::vector<App>();
				std::vector<string> onServer;
				if (this->IsAdmin) {
					httplib::Client cli("https://" + this->Uri);
					auto res = cli.Get("/pipeline/v2/update/apps.json");
					if (res->status == 200)
					{
						Json::Value root;
						Json::Reader reader;
						reader.parse(res->body, root);

						for (auto obj : root["apps"]) {
							fs::create_directories(GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->Uri) + L"\\" + to_wstring(obj["name"].asString()));
							onServer.push_back(obj["name"].asString());
						}
					}
				}

				if (fs::exists(GetExecutableFolder().wstring() + L"\\updates\\"))
				{
					for (auto obj : fs::directory_iterator(GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->Uri) + L"\\"))
					{
						if (!fs::is_directory(obj))
							continue;
						App newVersion;
						newVersion.Id = obj.path().filename().string();
						newVersion.Host = this;
						newVersion.OnServer = std::find(onServer.begin(), onServer.end(), newVersion.Id) != onServer.end();
						ret.push_back(newVersion);
					}
					this->Apps = ret;
				}
				this->WaitingGetApps = false;
			});
	}
	return &this->Apps;
}

KeyManager::Key UpdateManager::Host::GetKey(string name)
{
	for (auto group : this->accessGroups) {
		for (auto key : group->Keys) {
			if (key.Name == name)
				return key;
		}
	}
	return KeyManager::Key();
}

std::optional<Json::Value> GetDetailsJSON(string host, string ver, string ghub) {
	httplib::Client cli("https://" + host);
	auto res = cli.Get("/pipeline/" + ver + "/update/" + ghub + "/win/canary/details.json");
	if (res->status != 200)
		return nullopt;
	Json::Value root;
	Json::Reader reader;
	reader.parse(res->body, root);
	return root;
}

std::optional<Json::Value> GetAccessGroupJSON(string host, string accessGroup) {
	httplib::Client cli("https://" + host);
	auto res = cli.Get("https://updates.ghub.logitechg.com/pipeline/v2/access/" + accessGroup + "/content.json");
	if (res->status != 200)
		return nullopt;
	Json::Value root;
	Json::Reader reader;
	reader.parse(res->body, root);
	return root;
}

std::optional<Json::Value> GetJSONFromString(string json) {
	/*if (res->status != 200)
		return nullopt;*/
	Json::Value root;
	Json::Reader reader;
	reader.parse(json, root);
	return root;
}

void UpdateManager::App::AddBuild(string buildName)
{
	if (this->Host->IsAdmin) {
		Json::Value root;
		Json::Reader reader;
		httplib::Client cli("https://" + this->Host->Uri);
		string auth = this->Host->Login + ":" + this->Host->Password;
		httplib::Headers headers = {
  { "Authorization",  base64_encode((const BYTE*)auth.data(), auth.size())}
		};

		cli.Get("/pipeline/v2/update/app/" + this->Id + "/build/add/" + buildName);
	}
	this->GetBuilds(true);
}

vector<Build>* UpdateManager::App::GetBuilds(bool enforce)
{
	if (!enforce && this->Builds.size()) {
		return &this->Builds;
	}
	if (!this->WaitingGetBuilds) {
		this->WaitingGetBuilds = true;
		fGetBuilds[this] = std::async([&]() {
			const auto buildFolder = GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->Host->Uri) + L"\\" + to_wstring(this->Id) + L"\\";

			Log("Getting builds");

			string lastBuildId = "";
			std::vector<Build> ret = std::vector<Build>();
			auto lastBuild = GetDetailsJSON(this->Host->Uri, "v2", this->Id);
			if (lastBuild.has_value()) {
				lastBuildId = lastBuild.value()["buildId"].asString();
				if (lastBuildId == "0") {
					this->Builds = ret;
					return;
				}
				auto buildDirectory = buildFolder + to_wstring(lastBuildId);
				if (!fs::exists(buildDirectory))
					fs::create_directories(buildDirectory);

				WriteToFile(buildDirectory + L"\\details.json", lastBuild.value().toStyledString());
				Log("Found last build on the server: " + dye::light_green(lastBuildId));
			}


			if (fs::exists(GetExecutableFolder().wstring() + L"\\updates\\"))
			{
				for (auto obj : fs::directory_iterator(buildFolder))
				{
					if (!fs::is_directory(obj))
						continue;
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
							details = GetJSONFromString(ReadFromFile(buildDetailsDir));
							Log("Found local \"" + dye::light_yellow("details.json") + "\" for build: " + dye::aqua(details.value()["buildId"].asString()));
						}
					}

					if (details.has_value()) {
						if (details.value().isMember("keys") && details.value()["keys"].isMember("accessGroup")) {
							Log("Found Access Group " + dye::light_aqua(details.value()["keys"]["accessGroup"].asString()) + " in " + dye::aqua(newBuild->Id) + " build");
							std::optional<Json::Value> content = GetAccessGroupJSON(this->Host->Uri, details.value()["keys"]["accessGroup"].asString());
							if (content.has_value()) {
								KeyManager::LoadKeysFromJSON(this->Host, content.value());
							}
						}

						for (unsigned int i = 0; i < details.value()["depots"].size(); i++) {
							auto d = details.value()["depots"][i];
							BuildDepot newBuildFile;
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
			this->WaitingGetBuilds = false;
			});
	}
	return &this->Builds;
}

vector<BuildDepot>* UpdateManager::Build::GetDepots()
{
	return &this->Files;
}

bool UpdateManager::Build::HasDetails()
{
	if (!this->hasDetailsChecked) {
		this->hasDetails = fs::exists(GetExecutableFolder().wstring() + L"\\updates\\" + to_wstring(this->App->Host->Uri) + L"\\" + to_wstring(this->App->Id) + L"\\" + to_wstring(this->Id) + L"\\details.json");
		this->hasDetailsChecked = true;
	}
	return this->hasDetails;
}

void UpdateManager::BuildDepot::DownloadDepot(std::function<bool(uint64_t current, uint64_t total)> callback)
{
	Log("Downloading file " + this->Name);

	httplib::Client cli("https://" + this->Build->App->Host->Uri);
	httplib::Result res;
	if (callback)
		res = cli.Get(this->Url, callback);
	else
		res = cli.Get(this->Url);
	fs::create_directories(fs::path(this->FullPath).parent_path());
	WriteToFile(this->FullPath, res->body.data(), res->body.size());
	this->Downloaded = true;
}

BuildDepot::LoadResult UpdateManager::BuildDepot::LoadDepot(bool force)
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
		return this->lastLoadResult;
	}
	memset(this->Depot, 0, depotSize);
	ReadBinaryFile(this->FullPath, this->Depot, depotSize);

	this->FileType = *(BuildDepot::DFileType*)this->Depot;
	{
		int validEnum = (int)BuildDepot::DFileType::Default | (int)BuildDepot::DFileType::Encrypted | (int)BuildDepot::DFileType::EncryptedFile;// | (int)BuildFile::DFileType::Key;
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

void UpdateManager::BuildDepot::UnloadDepot()
{
	if (this->Depot != nullptr)
		free(this->Depot);
	this->Depot = nullptr;
	this->DepotSize = 0;
	this->lastLoadResult = BuildDepot::LoadResult::Null;
}

BuildDepot::UnpackResult UpdateManager::BuildDepot::CheckDepot(bool force)
{
	if (!force && this->lastCheckResult != UnpackResult::Null)
		return this->lastCheckResult;
	if (this->lastCheckResult == UnpackResult::Success)
		return UnpackResult::Success;
	if (this->Depot == nullptr)
	{
		BuildDepot::LoadResult loadResult = BuildDepot::LoadDepot();
		if (loadResult != BuildDepot::LoadResult::Success)
		{
			this->lastCheckResult = BuildDepot::UnpackResult::LoadError;
			return this->lastCheckResult;
		}
	}
	else {
		if (this->FileType == DFileType::Encrypted || this->FileType == DFileType::EncryptedFile)
		{
			Json::Value JSONData;
			unsigned int JSONLength = *(unsigned int*)(this->Depot + sizeof(BuildDepot::DFileType));
			{
				Json::Reader reader;

				string json = string(this->Depot + sizeof(BuildDepot::DFileType) + sizeof(unsigned int), this->Depot + sizeof(BuildDepot::DFileType) + sizeof(unsigned int) + JSONLength);
				reader.parse(json, JSONData);
			}

			this->sha = JSONData["header-sha"].asString();
			if (this->sha == "")
				this->sha = JSONData["file-sha"].asString();
			if (this->sha == "")
				this->sha = JSONData["sha"].asString();

			if (this->sha == "")
			{
				this->lastUnpackResult == UnpackResult::UnknownError;
				return this->lastUnpackResult;
			}
			string keyId = JSONData["key-id"].asString();
			KeyManager::Key key = this->Build->App->Host->GetKey(keyId);
			if (!key.IsValid())
			{
				this->lastUnpackResult = UnpackResult::KeyNotFound;
				return this->lastUnpackResult;
			}
			this->Key = key;
		}
	}

	if (fs::exists(this->UnpackedDir))
	{
		this->lastCheckResult = BuildDepot::UnpackResult::Success;
		return this->lastCheckResult;
	}
	this->lastCheckResult = BuildDepot::UnpackResult::NotUnpackedYet;
	return this->lastCheckResult;
}

BuildDepot::UnpackResult UpdateManager::BuildDepot::UnpackDepot(int* progress, int* progressMax, bool force)
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
	//else {
	//	if (CheckResult != UnpackResult::KeyNotFound) {
	//		this->lastUnpackResult = this->lastCheckResult;
	//		return this->lastUnpackResult;
	//	}
	//}

	Json::Value JSONData;
	unsigned int JSONLength = *(unsigned int*)(this->Depot + sizeof(BuildDepot::DFileType));
	{
		Json::Reader reader;

		string json = string(this->Depot + sizeof(BuildDepot::DFileType) + sizeof(unsigned int), this->Depot + sizeof(BuildDepot::DFileType) + sizeof(unsigned int) + JSONLength);
		reader.parse(json, JSONData);
	}

	// Reading files
	int offset = sizeof(BuildDepot::DFileType) + sizeof(unsigned int) + JSONLength;

	if (this->FileType == DFileType::EncryptedFile) {
		KeyManager::Key key = this->Build->App->Host->GetKey(JSONData["key-id"].asString());
		if (!key.IsValid())
		{
			this->lastUnpackResult == UnpackResult::KeyNotFound;
			return this->lastUnpackResult;
		}
		unsigned int fileSize = *(unsigned int*)(this->Depot + offset);
		offset += sizeof(unsigned int);
		string decryptedData = DecryptAES(string(this->Depot + offset, fileSize), key.Value, GetIV(JSONData["file-sha"].asString(), key.Name));
		fs::create_directories(this->UnpackedDir);
		WriteToFile(this->UnpackedDir + L"\\" + fs::path(this->Name).filename().wstring(), decryptedData.data(), decryptedData.size());
	}
	else
	{
		if (progress != nullptr)
			*progress = 0;
		if (progressMax)
			*progressMax = JSONData["files"].size();

		for (unsigned int i = 0; i < JSONData["files"].size() || offset < this->DepotSize; i++) {
			unsigned int fileSize = *(unsigned int*)(this->Depot + offset);
			offset += sizeof(unsigned int);
			if (this->FileType == DFileType::Encrypted && i == 0) {
				auto jsondata = DecryptAES(string(this->Depot + offset, fileSize), this->Key.Value, GetIV(this->sha, this->Key.Name));
				JSONData = GetJSONFromString(jsondata).value();
				offset += fileSize;
				continue;
			}

			string fileName = JSONData["files"][(this->FileType == DFileType::Encrypted ? i - 1 : i)]["name"].asString();
			wstring filePathStr = this->UnpackedDir + L"\\" + to_wstring(fileName);
			fs::path filePath = fs::path(filePathStr).parent_path();
			fs::create_directories(filePath);
			if (this->FileType == DFileType::Default)
				WriteToFile(this->UnpackedDir + L"\\" + to_wstring(fileName), this->Depot + offset, fileSize);
			else
			{
				string decryptedData = DecryptAES(string(this->Depot + offset, fileSize), this->Key.Value, GetIV(JSONData["files"][(this->FileType == DFileType::Encrypted ? i - 1 : i)]["sha"].asString(), this->Key.Name));
				WriteToFile(this->UnpackedDir + L"\\" + to_wstring(fileName), decryptedData.data(), decryptedData.size());
			}

			offset += fileSize;
			Log("Unpacked file " + fileName + ", Size: " + to_string(fileSize));
			if (progress != nullptr)
				(*progress)++;
		}
	}

	this->lastUnpackResult = UnpackResult::Success;
	this->CheckDepot();
	if (progress != nullptr)
		*progress = 0;
	if (progressMax != nullptr)
		*progressMax = 0;
	return UnpackResult::Success;
}

BuildDepot::UnpackResult UpdateManager::BuildDepot::UnpackDepot(bool force)
{
	return UnpackDepot(nullptr, nullptr, force);
}

void UpdateManager::BuildDepot::PackDepot()
{

}

void UpdateManager::KeyManager::LoadKeysFromJSON(UpdateManager::Host* host, Json::Value json)
{
	string group = json["accessGroup"].asString();
	for (auto obj : json["keys"]) {
		KeyManager::Key newKey;
		newKey.Name = obj["name"].asString();
		newKey.Value = obj["key"].asString();
		/*if (KeyManager::accessGroup[group].Key == newKey.Key && KeyManager::accessGroup[group].Name)
			KeyManager::accessGroup[group] = newKey;*/
		auto accessGroup = host->GetAccessGroup(group);
		if (accessGroup == nullptr)
			accessGroup = host->AddAccessGroup(group, group);
		for (auto obj : accessGroup->Keys) {
			if (obj.Value == newKey.Value && obj.Name == newKey.Name)
				continue;
		}
		host->GetAccessGroup(group)->Keys.push_back(newKey);
		Log("Added new key (" + dye::aqua(newKey.Name) + ") to accessGroup (" + dye::light_aqua(group) + ")");
	}
}

void UpdateManager::KeyManager::LoadKeysFromJSON(UpdateManager::Host* host, string json)
{
	// https://updates.ghub.logitechg.com/pipeline/v2/update/ghub10/win/public/details.json

}

bool UpdateManager::KeyManager::Key::IsValid()
{
	if (this->Name == "" || this->Value == "") // I know that it's not necessary to check name, but just in case
		return false;
	return true;
}

UpdateManager::Host::AccessGroup::AccessGroup(Host* host)
{
	this->host = host;
}

bool UpdateManager::Host::AccessGroup::HasKey(string name)
{
	for (auto obj : this->Keys) {
		if (obj.Name == name)
			return true;
	}
	return false;
}

void UpdateManager::Host::AccessGroup::AddKey(string name, string value, bool online)
{
	KeyManager::Key k;
	k.Name = name;
	k.Value = value;
	this->AddKey(k, online);
}

void UpdateManager::Host::AccessGroup::AddKey(KeyManager::Key key, bool online)
{
	if (this->HasKey(key.Name))
#ifdef KEYSDUPLICATES
		Log("Skipped check for key duplicates");
#endif
	{
		if (online && this->host) {
			httplib::Client cli("https://" + this->host->Uri);
			string auth = this->host->Login + ":" + this->host->Password;
			httplib::Headers headers = {
	  { "Authorization",  base64_encode((const BYTE*)auth.data(), auth.size())}
			};
			httplib::Result res = cli.Get("/pipeline/v2/update/access_group/" + this->Value + "/key/add/" + key.Name + "/" + key.Value, headers);
		}

		KeyManager::Key k;
		k.Name = key.Name;
		k.Value = key.Value;
		this->Keys.push_back(k);
	}
}
