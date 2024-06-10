#include "Settings.h"
#include "UpdateManager/UpdateManager.h"
#include "UpdateManager/Utils/Utils.h"
#include "UpdateManager/Utils/ini/ini.h"
#include <string>

std::string settingsini;

void Settings::LoadSettings()
{
	settingsini = to_string(UpdateManager::GetExecutableFolder().wstring() + L"\\settings.ini");
	if (fs::exists(settingsini)) {
		inih::INIReader r{ settingsini };
		Settings::LowRAM = r.Get<bool>("settings", "low_ram", false);
		Settings::DarkMode = r.Get<bool>("settings", "dark_mode", true);
		Settings::ThreadsCount = r.Get<int>("settings", "threads_count", 1);
		Settings::AlwaysUnpackDepot = r.Get<bool>("settings", "always_unpack", true);
#ifdef _DEBUG
		Settings::ShowImGuiDemoWindow = r.Get<bool>("settings", "show_demo", true);
#endif
		Settings::Admin::SkipRemoveConfirmation = r.Get<bool>("settings", "skip_remove_confirmation", true);
	}
}

void Settings::SaveSettings()
{
	inih::INIReader r;
	r.InsertEntry<bool>("settings", "low_ram", Settings::LowRAM);
	r.InsertEntry<bool>("settings", "dark_mode", Settings::DarkMode);
	r.InsertEntry<int>("settings", "threads_count", Settings::ThreadsCount);
	r.InsertEntry<bool>("settings", "always_unpack", Settings::AlwaysUnpackDepot);
	r.InsertEntry<bool>("settings", "show_demo", Settings::ShowImGuiDemoWindow);
	r.InsertEntry<bool>("settings", "skip_remove_confirmation", Settings::Admin::SkipRemoveConfirmation);
	if (fs::exists(settingsini)) {
		fs::remove(settingsini);
	}
	inih::INIWriter::write(settingsini, r);
}
