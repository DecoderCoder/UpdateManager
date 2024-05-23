#pragma once
#include <string>
#include <iostream>
#include <chrono>
#include "colors.hpp"


#define LOGGING_ENABLED 1


#if LOGGING_ENABLED == 1
#define LogW(s) std::wcout << "[ " << std::chrono::system_clock::now() << " | "; cout << dye::light_yellow(__FUNCTION__); wcout << " ] " << s << std::endl;
#define Log(s) std::cout << "[ " << std::chrono::system_clock::now() << " | "; cout << dye::light_yellow(__FUNCTION__); cout << " ] " << s << std::endl;
#else
#define LogW(s)
#define Log(s)
#endif