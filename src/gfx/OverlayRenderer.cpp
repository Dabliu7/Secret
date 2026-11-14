#include "gfx/OverlayRenderer.h"

#include <cstdio>

#include "game/GameDraw.h"
#include "mtasa/Log.h"

namespace mtasa::gfx {

OverlayRenderer& OverlayRenderer::Get() {
    static OverlayRenderer instance;
    return instance;
}

bool OverlayRenderer::ComputeLayout(Layout* out) const {
    if (!game_draw_ || !game_draw_->ready()) return false;
    const float w = static_cast<float>(game_draw_->ScreenWidth());
    const float h = static_cast<float>(game_draw_->ScreenHeight());
    if (w <= 0.0f || h <= 0.0f) return false;

    out->banner = {w * 0.32f, h * 0.015f, w * 0.36f, h * 0.065f};
    // Botão ao lado da faixa, alto o bastante para um dedo.
    out->menu_button = {out->banner.x + out->banner.w + w * 0.01f, h * 0.015f, w * 0.09f, h * 0.10f};
    out->panel = {w * 0.25f, h * 0.22f, w * 0.50f, h * 0.56f};
    out->close_button = {out->panel.x + out->panel.w * 0.30f, out->panel.y + out->panel.h * 0.74f,
                         out->panel.w * 0.40f, out->panel.h * 0.18f};
    return true;
}

OverlayRenderer::Target OverlayRenderer::HitTest(const Layout& layout, float x, float y) const {
    if (panel_open_) {
        if (layout.close_button.Contains(x, y)) return Target::CloseButton;
        // Com o painel aberto, todo toque é da UI: o jogo não deve reagir por baixo.
        return Target::Panel;
    }
    if (layout.menu_button.Contains(x, y)) return Target::MenuButton;
    return Target::None;
}

void OverlayRenderer::RenderFrame() {
    if (frame_++ == 0) {
        LOGI("primeiro frame de overlay (game_draw=%d, backend=%p)",
             game_draw_ && game_draw_->ready(), static_cast<void*>(backend_));
    }
    Layout layout;
    if (ComputeLayout(&layout)) {
        DrawStatusBanner(layout);
        if (panel_open_) DrawPanel(layout);
    }
    if (!backend_) return;
    // TODO(fase 4): CEGUI::System::renderGUI() + fila de dxDraw* dos scripts.
}

// Faixa de status no topo da tela: prova visual de que o overlay está vivo.
void OverlayRenderer::DrawStatusBanner(const Layout& layout) {
    const float h = static_cast<float>(game_draw_->ScreenHeight());
    const Box& b = layout.banner;
    game_draw_->Rect(b.x, b.y, b.w, b.h, 0x000000A0);
    game_draw_->Rect(b.x, b.y + b.h - h * 0.006f, b.w, h * 0.006f, 0xF5A623FF);

    char text[96];
    std::snprintf(text, sizeof(text), "MTA:SA Android %s   %d FPS", MTASA_VERSION,
                  static_cast<int>(game_draw_->Fps() + 0.5f));
    game_draw_->Text(b.x + b.w * 0.05f, b.y + b.h * 0.12f, 0.6f, 0xFFFFFFFF, text);

    const Box& m = layout.menu_button;
    game_draw_->Rect(m.x, m.y, m.w, m.h, panel_open_ ? 0xF5A623E0 : 0x000000A0);
    game_draw_->Rect(m.x, m.y + m.h - h * 0.006f, m.w, h * 0.006f, 0xF5A623FF);
    game_draw_->Text(m.x + m.w * 0.18f, m.y + m.h * 0.25f, 0.7f, 0xFFFFFFFF, "MTA");
}

void OverlayRenderer::DrawPanel(const Layout& layout) {
    const float h = static_cast<float>(game_draw_->ScreenHeight());
    const Box& p = layout.panel;
    game_draw_->Rect(p.x, p.y, p.w, p.h, 0x101418E6);
    game_draw_->Rect(p.x, p.y, p.w, h * 0.008f, 0xF5A623FF);

    const float left = p.x + p.w * 0.06f;
    game_draw_->Text(left, p.y + p.h * 0.07f, 0.9f, 0xF5A623FF, "Multi Theft Auto");
    game_draw_->Text(left, p.y + p.h * 0.26f, 0.6f, 0xFFFFFFFF, "Overlay: ok");
    game_draw_->Text(left, p.y + p.h * 0.38f, 0.6f, 0xFFFFFFFF, "Toque: ok");
    game_draw_->Text(left, p.y + p.h * 0.50f, 0.6f, 0xB0B0B0FF, "Rede: nao implementada");

    const Box& c = layout.close_button;
    game_draw_->Rect(c.x, c.y, c.w, c.h, 0xF5A623FF);
    game_draw_->Text(c.x + c.w * 0.28f, c.y + c.h * 0.22f, 0.7f, 0x000000FF, "Fechar");
}

// Ação 2 = dedo na tela (início OU movimento), 1 e 4 = dedo saiu. Um gesto que
// começa sobre a UI é consumido inteiro; o jogo nunca vê parte dele.
bool OverlayRenderer::OnTouch(int action, int pointer, int x, int y) {
    if (pointer < 0 || pointer >= kMaxPointers) return false;
    Layout layout;
    if (!ComputeLayout(&layout)) return false;

    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    if (touch_log_budget_ > 0) {
        --touch_log_budget_;
        LOGD("toque: acao=%d ponteiro=%d pos=(%d,%d) tela=%dx%d", action, pointer, x, y,
             game_draw_->ScreenWidth(), game_draw_->ScreenHeight());
    }

    if (action == 2) {
        if (!pointer_down_[pointer]) {
            pointer_down_[pointer] = true;
            pointer_target_[pointer] = HitTest(layout, fx, fy);
        }
        return pointer_target_[pointer] != Target::None;
    }

    if (!pointer_down_[pointer]) return false;
    const Target pressed = pointer_target_[pointer];
    pointer_down_[pointer] = false;
    pointer_target_[pointer] = Target::None;
    if (pressed == Target::None) return false;

    // Só dispara se o dedo soltou sobre o mesmo elemento em que apertou.
    if (action == 1 && HitTest(layout, fx, fy) == pressed) {
        if (pressed == Target::MenuButton) {
            panel_open_ = true;
            LOGI("painel MTA aberto");
        } else if (pressed == Target::CloseButton) {
            panel_open_ = false;
            LOGI("painel MTA fechado");
        }
    }
    return true;
}

}  // namespace mtasa::gfx
