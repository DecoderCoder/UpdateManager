#pragma once
#include <string>
#include <fstream>
#include <Shlobj.h>
#include <vector>

using namespace std;

const char HexLower[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b','c','d','e','f' };

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

static string ReadFromFile(wstring filename) {
	std::ifstream in(filename);
	std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return contents;
}

static void ReadBinaryFile(wstring filename, char* dst, size_t size) {
	std::ifstream input(filename, std::ios::binary);
	input.read(dst, size);
	input.close();
}

static void ReadBinaryFile(wstring filename, char** dst, size_t& size) {
	std::ifstream input(filename, std::ios::ate | std::ios::in | std::ios::binary);
	size = input.tellg();
	input.seekg(0);
	*dst = (char*)malloc(size); // memory leak, but idc ;D
	memset(*dst, 0, size);
	input.read(*dst, size);
	input.close();
}

static string url_encode(const string& value) {
	ostringstream escaped;
	escaped.fill('0');
	escaped << hex;

	for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		string::value_type c = (*i);

		// Keep alphanumeric and other accepted characters intact
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
			continue;
		}

		// Any other characters are percent-encoded
		escaped << uppercase;
		escaped << '%' << setw(2) << int((unsigned char)c);
		escaped << nouppercase;
	}

	return escaped.str();
}

static std::string ToHex(char* input, size_t size, bool upperCase) {
	const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B','C','D','E','F' };
	const char hex2[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b','c','d','e','f' };
	std::string output;
	for (size_t i = 0; i < size; i++) {
		if (upperCase) {
			output.append(&hex[(input[i] & 0xF0) >> 4], 1);
			output.append(&hex[input[i] & 0xF], 1);
		}
		else {
			output.append(&hex2[(input[i] & 0xF0) >> 4], 1);
			output.append(&hex2[input[i] & 0xF], 1);
		}

		output.append(" ");
		if ((i + 1) % 16 == 0 && i != 0)
			output.append("\r\n");
	}
	return output;
}

static std::string ToHex2(char* input, size_t size, bool upperCase) { // a lot faster than ToHex impl in debug mode
	const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B','C','D','E','F' };
	const char hex2[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b','c','d','e','f' };
	size_t retSize = size * 3 + (size / 16) * 2 + 1 + 2;
	char* output = (char*)malloc(retSize);

	int offst = 0;
	for (size_t i = 0; i < size; i++) {
		if (upperCase) {
			output[offst++] = hex[(input[i] & 0xF0) >> 4];
			output[offst++] = hex[(input[i] & 0xF)];

		}
		else {
			output[offst++] = hex2[(input[i] & 0xF0) >> 4];
			output[offst++] = hex2[(input[i] & 0xF)];
		}

		output[offst++] = ' ';
		if ((i + 1) % 16 == 0 && i != 0)
		{
			output[offst++] = '\r';
			output[offst++] = '\n';
		}
	}
	auto ret = string(output, offst);
	free(output);
	return ret;
}

static void OpenFolder(std::wstring folder, std::vector<std::wstring> selectedFiles) {
	PIDLIST_ABSOLUTE pidl;
	if (SUCCEEDED(SHParseDisplayName(folder.c_str(), 0, &pidl, 0, 0)))
	{
		std::vector<LPITEMIDLIST> files;
		for (const auto& file : selectedFiles) {
			files.push_back(ILCreateFromPath(file.c_str())); // same as SHParseDisplayName
		}

		SHOpenFolderAndSelectItems(pidl, files.size(), (LPCITEMIDLIST*)files.data(), 0);
		ILFree(pidl);
		for (const auto& file : files) {
			ILFree(file);
		}
	}
}

static void OpenFolder(std::wstring folder, std::wstring selectedFile) {
	std::vector<std::wstring> file;
	file.push_back(selectedFile);
	OpenFolder(folder, file);
}

static void OpenFolder(std::wstring folder) {
	std::vector<std::wstring> empty;
	OpenFolder(folder, empty);
}

static std::vector<std::filesystem::path> GetFiles(wstring path) {
	std::vector<std::filesystem::path> result;

	for (auto obj : std::filesystem::directory_iterator(path)) {
		if (std::filesystem::is_directory(obj)) {
			for (auto obj2 : GetFiles(obj.path().wstring())) {
				result.push_back(obj2);
			}
		}
		result.push_back(obj.path());
	}

	return result;
}