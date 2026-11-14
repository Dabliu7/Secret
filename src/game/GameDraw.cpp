#include "game/GameDraw.h"

#include <cstring>

#include "game/Symbols.h"
#include "mtasa/Log.h"

namespace mtasa::game {
namespace {

template <typename T>
bool Bind(const hook::ModuleInfo& game, const char* symbol, T* out) {
    void* address = Resolve(game, symbol);
    if (!address) {
        LOGE("GameDraw: símbolo ausente: %s", symbol);
        return false;
    }
    std::memcpy(out, &address, sizeof(address));
    return true;
}

}  // namespace

bool GameDraw::Init(const hook::ModuleInfo& game) {
    bool ok = true;
    ok &= Bind(game, "RsGlobal", &rs_global_);
    ok &= Bind(game, "_ZN6CTimer8game_FPSE", &game_fps_);
    ok &= Bind(game, "_ZN9CSprite2d8DrawRectERK5CRectRK5CRGBA", &draw_rect_);
    ok &= Bind(game, "_ZN5CFont12SetFontStyleEh", &font_set_style_);
    ok &= Bind(game, "_ZN5CFont10SetJustifyEh", &font_set_justify_);
    ok &= Bind(game, "_ZN5CFont13SetBackgroundEhh", &font_set_background_);
    ok &= Bind(game, "_ZN5CFont8SetWrapxEf", &font_set_wrapx_);
    ok &= Bind(game, "_ZN5CFont8SetScaleEf", &font_set_scale_);
    ok &= Bind(game, "_ZN5CFont15SetProportionalEh", &font_set_proportional_);
    ok &= Bind(game, "_ZN5CFont14SetOrientationEh", &font_set_orientation_);
    ok &= Bind(game, "_ZN5CFont19SetRightJustifyWrapEf", &font_set_right_justify_wrap_);
    ok &= Bind(game, "_ZN5CFont7SetEdgeEa", &font_set_edge_);
    ok &= Bind(game, "_ZN5CFont12SetDropColorE5CRGBA", &font_set_drop_color_);
    ok &= Bind(game, "_ZN5CFont8SetColorE5CRGBA", &font_set_color_);
    ok &= Bind(game, "_ZN5CFont11PrintStringEffPs", &font_print_string_);
    ok &= Bind(game, "_ZN5CFont16RenderFontBufferEv", &font_render_buffer_);
    ready_ = ok;
    LOGI("GameDraw: %s", ok ? "pronto" : "desligado (símbolos ausentes)");
    return ok;
}

int GameDraw::ScreenWidth() const {
    int v = 0;
    std::memcpy(&v, rs_global_ + 0x8, sizeof(v));
    return v;
}

int GameDraw::ScreenHeight() const {
    int v = 0;
    std::memcpy(&v, rs_global_ + 0xC, sizeof(v));
    return v;
}

float GameDraw::Fps() const { return *game_fps_; }

GameDraw::CRGBA GameDraw::ToRGBA(uint32_t rgba) {
    return {static_cast<uint8_t>(rgba >> 24), static_cast<uint8_t>(rgba >> 16),
            static_cast<uint8_t>(rgba >> 8), static_cast<uint8_t>(rgba)};
}

void GameDraw::Rect(float x, float y, float w, float h, uint32_t rgba) const {
    const CRect rect{x, y, x + w, y + h};
    const CRGBA color = ToRGBA(rgba);
    draw_rect_(&rect, &color);
}

void GameDraw::Text(float x, float y, float scale, uint32_t rgba, const char* ascii) const {
    // Texto do jogo é GXT de 16 bits; ASCII converte por simples alargamento.
    uint16_t gxt[256];
    size_t n = 0;
    for (; ascii[n] && n < 255; ++n) gxt[n] = static_cast<uint8_t>(ascii[n]);
    gxt[n] = 0;

    const CRGBA color = ToRGBA(rgba);
    const CRGBA black{0, 0, 0, color.a};
    const float screen_w = static_cast<float>(ScreenWidth());
    const float screen_h = static_cast<float>(ScreenHeight());

    // Mesma sequência que o jogo usa para o nome da estação de rádio.
    font_set_style_(2);
    font_set_justify_(0);
    font_set_background_(0, 0);
    font_set_wrapx_(screen_w);
    font_set_scale_(screen_h / 448.0f * 0.9f * scale);
    font_set_proportional_(1);
    font_set_orientation_(1);
    font_set_right_justify_wrap_(0.0f);
    font_set_edge_(1);
    font_set_drop_color_(&black);
    font_set_color_(&color);
    font_print_string_(x, y, gxt);
    font_render_buffer_();
}

}  // namespace mtasa::game
