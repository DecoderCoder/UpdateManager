#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace Utils {
	static bool IsUUID(std::string str) {
		bool is = false;
		for (int i = 0; i < str.size(); i++) {
			if (i == 8 || i == 13 || i == 18 || i == 23)
				continue;
			if (!(str[i] >= 48 && str[i] <= 57 || str[i] >= 97 && str[i] <= 102))
			{
				is = true;
				break;
			}
		}
		if (str.size() != 36)
			is = true;
		if (!is)
			is = !(str[8] == '-' && str[13] == '-' && str[18] == '-' && str[23] == '-');
		return !is;
	}

	static std::vector<unsigned char> hex_to_bytes(std::string input, bool spaces = false) {
		std::vector<unsigned char> result;
		for (unsigned i = 0, uchr; i < input.length(); i += 2) {
			sscanf(input.c_str() + i, "%2x", &uchr); // conversion
			result.push_back(uchr); // save as char
		}
		return result;
	}
}