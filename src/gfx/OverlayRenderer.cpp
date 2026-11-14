#include "gfx/OverlayRenderer.h"

#include <cstdio>

#include "game/GameDraw.h"
#include "mtasa/Log.h"

namespace mtasa::gfx {

OverlayRenderer& OverlayRenderer::Get() {
    static OverlayRenderer instance;
    return instance;
}

void OverlayRenderer::RenderFrame() {
    if (frame_++ == 0) {
        LOGI("primeiro frame de overlay (game_draw=%d, backend=%p)",
             game_draw_ && game_draw_->ready(), static_cast<void*>(backend_));
    }
    DrawStatusBanner();
    if (!backend_) return;
    // TODO(fase 4): CEGUI::System::renderGUI() + fila de dxDraw* dos scripts.
}

// Faixa de status no topo da tela: prova visual de que o overlay está vivo.
void OverlayRenderer::DrawStatusBanner() {
    if (!game_draw_ || !game_draw_->ready()) return;
    const float w = static_cast<float>(game_draw_->ScreenWidth());
    const float h = static_cast<float>(game_draw_->ScreenHeight());
    if (w <= 0.0f || h <= 0.0f) return;

    const float box_w = w * 0.36f;
    const float box_h = h * 0.065f;
    const float box_x = (w - box_w) * 0.5f;
    const float box_y = h * 0.015f;
    game_draw_->Rect(box_x, box_y, box_w, box_h, 0x000000A0);
    game_draw_->Rect(box_x, box_y + box_h - h * 0.006f, box_w, h * 0.006f, 0xF5A623FF);

    char text[96];
    std::snprintf(text, sizeof(text), "MTA:SA Android %s   %d FPS", MTASA_VERSION,
                  static_cast<int>(game_draw_->Fps() + 0.5f));
    game_draw_->Text(box_x + box_w * 0.05f, box_y + box_h * 0.12f, 0.6f, 0xFFFFFFFF, text);
}

bool OverlayRenderer::OnTouch(int action, int pointer, int x, int y) {
    // TODO(fase 4): injetar em CEGUI (injectMousePosition/ButtonDown/Up).
    (void)action; (void)pointer; (void)x; (void)y;
    return capture_input_;
}

}  // namespace mtasa::gfx
