#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#include <map>
#include <string>
#include <filesystem>
#include "Utils/Logger.h"
#include "Utils/Utils.h"
#include "Utils/Encryption.h"
#include "Utils/jsoncpp/json/json.h"

using namespace std;
namespace fs = std::filesystem;

namespace UpdateManager {
	class Build;
	class App;
	class Host;
	class BuildFile;

	namespace KeyManager {
		class Key;

		class Key {
		public:
			string Name;
			string Key;

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

	class BuildFile
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
		//	Key = 0x20210521,
			EncryptedFile = 0x20210521,
			Unknown = 0
		};

		string sha;
		KeyManager::Key Key;

		string DFileTypeToString(BuildFile::DFileType type);

		Build* Build;
		string Name;
		wstring FullPath;
		wstring UnpackedDir;
		string Url;
		DFileType FileType; //Depot File Type

		char* Depot;
		size_t DepotSize;
		bool Downloaded;

		void DownloadDepot();
		LoadResult LoadDepot(bool force = true);
		void UnloadDepot();
		UnpackResult GetFileType();
		UnpackResult CheckDepot(bool force = true);
		UnpackResult UnpackDepot(int* file, int* fileCount, bool force = true);
		UnpackResult UnpackDepot(bool force = true);
	};

	class Build {
	private:
		bool hasDetails = false;
		bool hasDetailsChecked = false;
	public:
		string Id;
		App* App;
		bool LastBuild;

		vector<BuildFile> Files;

		vector<BuildFile>* GetFiles();

		bool HasDetails();
	};

	class App {
	public:
		string Id;
		Host* Host;
		vector<Build> Builds;

		vector<Build>* GetBuilds(bool enforce = false);
	};

	class Host {
	public:
		bool IsAdmin = false;
		string Uri = "";
		vector<App> Apps;

		map<string, std::vector<KeyManager::Key>> accessGroup;

		vector<App>* GetVersions(bool enforce = false);
		KeyManager::Key GetKey(string name);
	};

	inline std::vector<Host> Hosts;

	std::vector<Host>* GetHosts(bool enforce = false);

	fs::path GetExecutableFolder();

	string GetUpdateManagerPath(UpdateManager::Host host);
	string GetUpdateManagerPath(UpdateManager::App app);
	string GetUpdateManagerPath(UpdateManager::Build build);
	string GetUpdateManagerPath(UpdateManager::BuildFile buildFile);
}

using namespace UpdateManager;