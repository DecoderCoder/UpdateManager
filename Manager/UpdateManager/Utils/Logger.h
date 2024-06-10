#pragma once
#include <string>
#include <iostream>
#include <chrono>
#include "colors.hpp"
#include <mutex>



#define LOGGING_ENABLED 1
#define LOGGINGTHREADSAFE_ENABLED 1

#if LOGGINGTHREADSAFE_ENABLED == 1
inline std::mutex log_lock;
#endif

#if LOGGING_ENABLED == 1
#if LOGGINGTHREADSAFE_ENABLED == 1
#define LogW(s) {log_lock.lock(); std::wcout << "[ " << std::chrono::system_clock::now() << " | "; cout << dye::light_yellow(__FUNCTION__); wcout << " ] " << s << std::endl; log_lock.unlock();}
#define Log(s) {log_lock.lock(); std::cout << "[ " << std::chrono::system_clock::now() << " | "; cout << dye::light_yellow(__FUNCTION__); cout << " ] " << s << std::endl; log_lock.unlock();}
#else
#define LogW(s) std::wcout << "[ " << std::chrono::system_clock::now() << " | "; cout << dye::light_yellow(__FUNCTION__); wcout << " ] " << s << std::endl;
#define Log(s) std::cout << "[ " << std::chrono::system_clock::now() << " | "; cout << dye::light_yellow(__FUNCTION__); cout << " ] " << s << std::endl;
#endif
#else
#define LogW(s)
#define Log(s)
#endif