#pragma once
#include "imgui.h"

namespace nl::gfx {
class Blur;
}

namespace nl::ui {

[[nodiscard]] bool QueueBlurCapture(gfx::Blur& blur, ImDrawList* dl, bool dirty);
void BlitBlurSolid(gfx::Blur& blur, ImDrawList* dl, float x0, float y0, float x1, float y1,
                   float alpha, float dispW, float dispH, float rounding);

} // namespace nl::ui
