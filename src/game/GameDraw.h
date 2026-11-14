#pragma once
#include <cstdint>

#include "mtasa/Hook.h"

namespace mtasa::game {

// Desenho 2D usando as rotinas do próprio jogo (CSprite2d e CFont). Só pode ser
// chamado no thread do jogo, dentro do hook de Render2dStuff.
// Assinaturas e offsets conferidos na libGame.so 2.11.311.
class GameDraw {
public:
    // Resolve os símbolos. Retorna false se algum faltar (o overlay fica desligado).
    bool Init(const hook::ModuleInfo& game);
    bool ready() const { return ready_; }

    int ScreenWidth() const;
    int ScreenHeight() const;
    float Fps() const;

    // Cores em 0xRRGGBBAA. Coordenadas em pixels, origem no canto superior esquerdo.
    void Rect(float x, float y, float w, float h, uint32_t rgba) const;
    // `scale` 1.0 equivale ao tamanho de texto do HUD para a resolução atual.
    void Text(float x, float y, float scale, uint32_t rgba, const char* ascii) const;

private:
    struct CRGBA {
        uint8_t r, g, b, a;
    };
    struct CRect {
        float left, top, right, bottom;
    };
    static CRGBA ToRGBA(uint32_t rgba);

    bool ready_ = false;
    const uint8_t* rs_global_ = nullptr;  // +0x8 largura (int), +0xC altura (int)
    const float* game_fps_ = nullptr;

    // O ABI passa CRGBA por ponteiro mesmo quando a assinatura C++ é por valor.
    void (*draw_rect_)(const CRect*, const CRGBA*) = nullptr;
    void (*font_set_style_)(uint8_t) = nullptr;
    void (*font_set_justify_)(uint8_t) = nullptr;
    void (*font_set_background_)(uint8_t, uint8_t) = nullptr;
    void (*font_set_wrapx_)(float) = nullptr;
    void (*font_set_scale_)(float) = nullptr;
    void (*font_set_proportional_)(uint8_t) = nullptr;
    void (*font_set_orientation_)(uint8_t) = nullptr;  // 0 centro, 1 esquerda, 2 direita
    void (*font_set_right_justify_wrap_)(float) = nullptr;
    void (*font_set_edge_)(int8_t) = nullptr;
    void (*font_set_drop_color_)(const CRGBA*) = nullptr;
    void (*font_set_color_)(const CRGBA*) = nullptr;
    void (*font_print_string_)(float, float, const uint16_t*) = nullptr;
    void (*font_render_buffer_)() = nullptr;
};

}  // namespace mtasa::game
