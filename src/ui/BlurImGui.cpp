#include "BlurImGui.h"
#include "../gfx/Blur.h"
#include <algorithm>

namespace nl::ui {
namespace {

void OnBlurBegin(const ImDrawList*, const ImDrawCmd* cmd) {
    auto* blur = static_cast<gfx::Blur*>(cmd->UserCallbackData);
    if (!blur) return;
    blur->ExecutePending();
}

}

bool QueueBlurCapture(gfx::Blur& blur, ImDrawList* dl, bool dirty) {
    if (!dl || !blur.QueueCapture(dirty)) return false;
    if (blur.IsCapturePending()) {
        dl->AddCallback(OnBlurBegin, &blur);
        dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }
    return blur.HasEffect() || blur.IsCapturePending();
}

void BlitBlurSolid(gfx::Blur& blur, ImDrawList* dl, float x0, float y0, float x1, float y1,
                   float alpha, float dispW, float dispH, float rounding) {
    auto* srv = blur.EffectSrv();
    if (!dl || !srv || x1 <= x0 || y1 <= y0) return;
    if (dispW < 1.f || dispH < 1.f) return;
    alpha = std::clamp(alpha, 0.f, 1.f);
    if (alpha < 0.02f) return;
    dl->AddImageRounded(reinterpret_cast<ImTextureID>(srv), ImVec2(x0, y0), ImVec2(x1, y1),
                        ImVec2(x0 / dispW, y0 / dispH), ImVec2(x1 / dispW, y1 / dispH),
                        IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.f + 0.5f)), rounding);
}

}
