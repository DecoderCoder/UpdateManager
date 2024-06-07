#pragma once

namespace Settings {
	inline bool DarkMode = true;
	inline int ThreadsCount = 1;
	inline bool UseSSL = true;

	inline bool AlwaysUnpackDepot = true;

	namespace Admin {
		inline bool AskDownloadNew = true;
	}

#ifdef _DEBUG
	inline bool ShowImGuiDemoWindow = true;
#else
	inline bool ShowImGuiDemoWindow = false;
#endif

	void LoadSettings();
	void SaveSettings();
}