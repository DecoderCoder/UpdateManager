#include "AboutWindow.h"

bool AboutWindow::Render()
{
	// http://decodercoder.xyz/about/my_avatar.png
	ImGui::SetNextWindowSize(ImVec2(860, 565));
	if (Settings::DarkMode)
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.059, 0.059, 0.059, 1.f));
	ImGui::Begin("About creator", &this->Opened, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
	if (Settings::DarkMode)
		ImGui::PopStyleColor();
	if (!Window::Render()) {
		ImGui::End();
		return false;
	}

	ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x / 2 - 215 / 2, 38));
	if (Global::myAvatarImage)
		ImGui::Image(Global::myAvatarImage, ImVec2(215, 215));

	ImGui::PushFont(Global::fontMedium32);
	ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::CalcTextSize("Oleksandr Klievtsov").x / 2, 257));
	ImGui::Text("Oleksandr Klievtsov");
	ImGui::PopFont();

	ImGui::PushFont(Global::fontRegular16);
	if (Settings::DarkMode)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.627, 0.627, 0.627, 1));
	else
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.392, 0.392, 0.392, 1));

	ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::CalcTextSize("@DecoderCoder").x / 2, 297));
	ImGui::Text("@DecoderCoder");

	ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x / 2 - 263 / 2, 330));
	if (Global::ddmaImage) {

		if (Settings::DarkMode) {
			auto draw = ImGui::GetWindowDrawList();
			auto pos = ImGui::GetWindowPos();
			draw->AddRectFilled(ImVec2(pos.x + ImGui::GetWindowContentRegionMax().x / 2 - 160, pos.y + 344), ImVec2(pos.x + ImGui::GetWindowContentRegionMax().x / 2 + 160, pos.y + 490), ImColor::ImColor(255, 255, 255), 15);
		}

		ImGui::Image(Global::ddmaImage, ImVec2(263, 148));
	}

	ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x / 2 - ImGui::CalcTextSize("Copyright (c) 2024 © All rights reserved").x / 2, 530));
	ImGui::Text("Copyright (c) 2024 | All rights reserved");

	ImGui::PopStyleColor();
	ImGui::PopFont();

	ImGui::End();
	return true;
}
