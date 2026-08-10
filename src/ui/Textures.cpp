#include "UiGpuResources.h"
#include "Widgets.h"
#include "Theme.h"
#include "../Texture.h"
#include "../Embedded.h"
#include "../Log.h"
#include "imgui.h"

namespace nl::ui {

void ReleaseTextures(UiGpuResources& gpu) {
    gpu.Release();
}

void LoadTextures(UiGpuResources& gpu, ID3D11Device* device, ID3D11DeviceContext* ctx) {
    const auto avatar = nl::embed::Avatar();
    const auto cs2 = nl::embed::Cs2();
    const auto csgo = nl::embed::Csgo();
    const auto valorant = nl::embed::Valorant();
    const auto apex = nl::embed::Apex();

    if (!gfx::LoadCircularTextureFromMemory(avatar.data, avatar.size, device, ctx, gpu.avatar))
        Log("Failed to load avatar texture");
    if (!gfx::LoadTextureFromMemory(cs2.data, cs2.size, device, gpu.texCs2))
        Log("Failed to load CS2 texture");
    if (!gfx::LoadTextureFromMemory(csgo.data, csgo.size, device, gpu.texCsgo))
        Log("Failed to load CSGO texture");
    if (!gfx::LoadTextureFromMemory(valorant.data, valorant.size, device, gpu.texValorant))
        Log("Failed to load Valorant texture");
    if (!gfx::LoadTextureFromMemory(apex.data, apex.size, device, gpu.texApex))
        Log("Failed to load Apex texture");
}

void DrawSplash(Theme& theme, float timeSec, float loaderAlpha) {
    if (loaderAlpha < 0.01f) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##splash", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetWindowPos();
    ImVec2 sz = ImGui::GetWindowSize();
    ImVec2 center(c.x + sz.x * 0.5f, c.y + sz.y * 0.5f);

    ImVec4 ac = theme.accent;
    ac.w = loaderAlpha;
    DrawSpinner(dl, center, 20.f, 4.f, ImGui::GetColorU32(ac), timeSec);
    ImGui::End();
}

}
