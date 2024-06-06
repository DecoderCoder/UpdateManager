#pragma once
#include "DirectX/DirectX.h"

namespace Global {
	inline std::pair<unsigned char*, size_t> myAvatar;
	inline std::pair<unsigned char*, size_t> ddma;
	inline std::pair<unsigned char*, size_t> fontRegular;
	inline std::pair<unsigned char*, size_t> fontMedium;

	inline ImFont* fontRegular16 = nullptr;
	inline ImFont* fontMedium32 = nullptr;

	inline ID3D11ShaderResourceView* myAvatarImage = nullptr;
	inline ID3D11ShaderResourceView* ddmaImage = nullptr;
}