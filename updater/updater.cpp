#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <string>
#include "jsoncpp/json/json.h"
#include "Encryption.h"
#include "base64.hpp"
#include <filesystem>
#include "httplib.h"

using namespace std;
namespace fs = std::filesystem;

char Host[] = "decodercoder.xyz||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";
char AppName[] = "test_app||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";
char MainName[] = "core|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";

fs::path GetExecutableFolder()
{
	wchar_t tPath[255];
	if (GetModuleFileName(GetModuleHandle(NULL), (LPWSTR)&tPath, 255) != 0) {
		wstring strPath = wstring(tPath);
		fs::path path = fs::path(strPath).parent_path();
		return path;
	}
	return fs::path();
}

inline wstring to_wstring(string str) {
	return std::wstring(str.begin(), str.end());
}

inline string to_string(wstring str) {
	return string(str.begin(), str.end());
}

static void WriteToFile(wstring filename, string text) {
	ofstream stream(filename);
	stream << text;
	stream.close();
}

static void WriteToFile(wstring filename, char* from, size_t size) {
	ofstream stream(filename, std::ios::binary);
	stream.write(from, size);
	stream.close();
}

std::optional<Json::Value> GetJSONFromString(string json) {
	/*if (res->status != 200)
		return nullopt;*/
	Json::Value root;
	Json::Reader reader;
	reader.parse(json, root);
	return root;
}

enum class DepotFileType {
	Default = 0x20170110,
	Encrypted = 0x20210506,
	EncryptedFile = 0x20210521,
	Unknown = 0
};

std::optional<std::pair<std::string, std::string>> GetKey(std::vector<std::pair<std::string, std::string>>& keys, std::string keyName) {
	for (auto& obj : keys)
	{
		if (obj.first == keyName)
			return obj;
	}
	return nullopt;
}

int main(int argc, char* argv[])
{
	std::string host = "";
	std::string appName = "";
	std::string downloadName = "";

	for (int i = 0; i < sizeof(Host); i++) {
		if (Host[i] == '|')
			break;
		host.push_back(Host[i]);
	}
	for (int i = 0; i < sizeof(AppName); i++) {
		if (AppName[i] == '|')
			break;
		appName.push_back(AppName[i]);
	}

	for (int i = 0; i < argc; i++) {
		if (string(argv[i]) == "-d")
		{
			downloadName = string(argv[i + 1]);
			break;
		}
	}



	if (downloadName == "")
		for (int i = 0; i < sizeof(MainName); i++) {
			if (MainName[i] == '|')
				break;
			downloadName.push_back(MainName[i]);
		}

	httplib::Client cli(host);
	auto res = cli.Get("/pipeline/v2/update/" + appName + "/win/public/details.json");
	auto details = GetJSONFromString(res->body).value();

	std::vector<std::pair<std::string, std::string>> keys;

	if (details.isMember("keys") && details["keys"].isMember("accessGroup")) {
		auto link = "/pipeline/v2/access/" + details["keys"]["accessGroup"].asString() + "/content.json";
		auto res2 = cli.Get("/pipeline/v2/access/" + details["keys"]["accessGroup"].asString() + "/content.json")->body;
		auto content = GetJSONFromString(res2).value();
		for (auto obj : content["keys"]) {
			keys.push_back(std::make_pair(obj["name"].asString(), obj["key"].asString()));
		}
	}
	bool downloaded = false;
	for (auto d : details["depots"])
	{
		std::string name = d["name"].asString();
		if (name == downloadName) {
			res = cli.Get(d["url"].asString());
			downloaded = true;
			break;
		}
	}
	if (!downloaded)
		return -1;
	size_t depotSize = res->body.size();
	char* depot = (char*)malloc(depotSize);
	memcpy(depot, res->body.data(), depotSize);

	Json::Value JSONData;
	unsigned int JSONLength = *(unsigned int*)(depot + sizeof(DepotFileType));
	{
		Json::Reader reader;

		string json = string(depot + sizeof(DepotFileType) + sizeof(unsigned int), depot + sizeof(DepotFileType) + sizeof(unsigned int) + JSONLength);
		reader.parse(json, JSONData);
	}

	// Reading files
	int offset = sizeof(DepotFileType) + sizeof(unsigned int) + JSONLength;
	//GetExecutableFolder
	DepotFileType fileType = *(DepotFileType*)depot;

	wstring unpackDir = GetExecutableFolder().wstring() + L"\\";
	string keyId = JSONData["key-id"].asString();

	auto key = GetKey(keys, keyId);
	auto sha = JSONData["header-sha"].asString();
	if (sha == "")
		sha = JSONData["file-sha"].asString();
	if (sha == "")
		sha = JSONData["sha"].asString();

	if (fileType == DepotFileType::Encrypted && !key.has_value())
		return 1;
	for (unsigned int i = 0; i < JSONData["files"].size() || offset < depotSize; i++) {
		unsigned int fileSize = *(unsigned int*)(depot + offset);
		offset += sizeof(unsigned int);
		if (fileType == DepotFileType::Encrypted && i == 0) {
			auto jsondata = DecryptAES(string(depot + offset, fileSize), key.value().second, GetIV(sha, key.value().first));
			JSONData = GetJSONFromString(jsondata).value();
			offset += fileSize;
			continue;
		}

		string fileName = JSONData["files"][(fileType == DepotFileType::Encrypted ? i - 1 : i)]["name"].asString();
		wstring filePathStr = unpackDir + L"\\" + to_wstring(fileName);
		fs::path filePath = fs::path(filePathStr).parent_path();
		fs::create_directories(filePath);
		if (fileType == DepotFileType::Default)
			WriteToFile(unpackDir + to_wstring(fileName), depot + offset, fileSize);
		else
		{
			string decryptedData = DecryptAES(string(depot + offset, fileSize), key.value().second, GetIV(JSONData["files"][(fileType == DepotFileType::Encrypted ? i - 1 : i)]["sha"].asString(), key.value().first));
			WriteToFile(unpackDir + to_wstring(fileName), decryptedData.data(), decryptedData.size());
		}

		offset += fileSize;
	}
}