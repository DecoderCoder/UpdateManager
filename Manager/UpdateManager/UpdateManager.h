#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#include <string>
#include <filesystem>
#include "Utils/Logger.h"
#include "Utils/Utils.h"

using namespace std;
namespace fs = std::filesystem;

namespace UpdateManager {
	class Build;
	class App;
	class Host;
	class BuildFile;

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
			Key = 0x20210521,
			EncryptedFile,
			Unknown = 0
		};

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
	public:
		string Id;
		App* App;
		bool LastBuild;

		vector<BuildFile> Files;

		vector<BuildFile>* GetFiles();
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

		vector<App>* GetVersions(bool enforce = false);
	};

	inline std::vector<Host> Hosts;

	std::vector<Host>* GetHosts(bool enforce = false);

	fs::path GetExecutableFolder();
}

using namespace UpdateManager;