#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <filesystem>
#include <future>
#include <mutex>
#include "Utils/Logger.h"
#include "Utils/Utils.h"
#include "Utils/Encryption.h"
#include "Utils/jsoncpp/json/json.h"
#include "Utils/ini/ini.h"
#include "Utils/base64/base64.h"

#define KEYSDUPLICATES

using namespace std;
namespace fs = std::filesystem;

namespace UpdateManager {
	class Build;
	class App;
	class Host;
	class AccessGroup;
	class BuildDepot;

	namespace KeyManager {
		class Key;

		class Key {
		public:
			string Name;
			string Value;

			bool IsValid();
		};

		void LoadKeysFromJSON(UpdateManager::Host* host, Json::Value json);
		void LoadKeysFromJSON(UpdateManager::Host* host, string json);

		void SaveKeys();
		void LoadKeys();
	}

	enum class DepotFileType {
		Default,
		Encrypted,
		EncryptedFile,
		Unknown
	};

	class DepotFile {

	};

	inline optional<fs::path> executableFolder = nullopt;

	//struct BuildFileStruct {
	//	
	//};

	class BuildDepot
	{
	public:
		enum class UnpackResult {
			Success,
			NotUnpackedYet,
			KeyNotFound,
			UnknownError,
			LoadError,
			Null
		};

		enum class LoadResult {
			Success,
			FileNotFound,
			UnknownFileType,
			UnknownError,
			Null
		};
	private:
		UnpackResult lastUnpackResult = UnpackResult::Null;
		UnpackResult lastCheckResult = UnpackResult::Null;
		LoadResult lastLoadResult = LoadResult::Null;
	public:

		enum class DFileType {
			Default = 0x20170110,
			Encrypted = 0x20210506,
			EncryptedFile = 0x20210521,
			Unknown = 0
		};

		string sha;
		KeyManager::Key Key;

		string DFileTypeToString(BuildDepot::DFileType type);

		Build* Build;
		string Name;
		wstring FullPath;
		wstring UnpackedDir;
		string Url;
		DFileType FileType; //Depot File Type

		char* Depot;
		size_t DepotSize;
		bool Downloaded;
		bool OnServer;

		void DownloadDepot(std::function<bool(uint64_t current, uint64_t total)> callback = nullptr);
		LoadResult LoadDepot(bool force = true);
		void UnloadDepot();
		UnpackResult GetFileType();
		UnpackResult CheckDepot(bool force = true);
		UnpackResult UnpackDepot(int* file, int* fileCount, bool force = true);
		UnpackResult UnpackDepot(bool force = true);

		void PackDepot();
	};

	class Build {
	private:
		bool hasDetails = false;
		bool hasDetailsChecked = false;
	public:
		string Id;
		App* App;
		bool LastBuild;

		vector<BuildDepot> Depots;

		vector<BuildDepot>* GetDepots();

		bool HasDetails();
		bool HasDepot(string name);
	};

	class App {
	public:
		bool WaitingGetBuilds = false;
		bool OnServer = false;
		string Id;
		Host* Host;
		vector<Build> Builds;
		void AddBuild(string buildName);
		vector<Build>* GetBuilds(bool enforce = false);
	};

	class Host {
	private:
	public:
		class AccessGroup {
			Host* host;
		public:
			string Name;
			string Value;

			AccessGroup(Host* host);

			bool HasKey(string name);
			void AddKey(string name, string value, bool online = false);
			void AddKey(KeyManager::Key key, bool online = false);

			std::vector<KeyManager::Key> Keys;
		};

		enum class AddAppResponse {
			AlreadyExists,
			HasDeleted,
			Success
		};

		enum class AddAccessGroupResponse {
			AlreadyExists,
			Success
		};

		bool WaitingGetApps = false;
		bool IsAdmin = false;

		string Login = "";
		string Password = "";

		string Uri = "";
		vector<App> Apps;

		std::vector<AccessGroup*> accessGroups;

		UpdateManager::Host::AccessGroup* GetAccessGroup(string value);
		bool HasAccessGroupName(string name);
		bool HasAccessGroup(string value);
		UpdateManager::Host::AccessGroup* AddAccessGroup(string name, string value, bool online = false, AddAccessGroupResponse* result = nullptr);

		AddAppResponse AddApp(string name, string accessGroupValue, int ifExists = -1);
		void RemoveApp(string name);
		vector<App>* GetApps(bool enforce = false);
		KeyManager::Key GetKey(string name);
	};

	inline std::vector<Host> Hosts;

	void AddHost(string host, bool isAdmin = false, string login = "", string password = "");
	void RemoveHost(string name);

	std::vector<Host>* GetHosts(bool enforce = false);

	fs::path GetExecutableFolder();

	string GetUpdateManagerPath(UpdateManager::Host host);
	string GetUpdateManagerPath(UpdateManager::App app);
	string GetUpdateManagerPath(UpdateManager::Build build);
	string GetUpdateManagerPath(UpdateManager::BuildDepot buildFile);
}

using namespace UpdateManager;