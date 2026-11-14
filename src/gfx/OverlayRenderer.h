#pragma once
#include <cstdint>

namespace mtasa::game {
class GameDraw;
}

namespace mtasa::gfx {

// Vértice 2D neutro: é o formato que CEGUI/dx* produzem. Cada backend converte
// para o seu formato nativo (RwIm2DVertex no caminho RenderWare).
struct Vertex2D {
    float x, y;
    float u, v;
    uint32_t rgba;
};

using TextureHandle = void*;  // RwRaster* no backend RenderWare

// Backend de desenho. A implementação alvo (fase 4) é sobre RwIm2D, para que os
// comandos entrem na fila de render do próprio jogo em vez de chamar GL direto.
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    virtual TextureHandle CreateTexture(int width, int height, const void* rgba8) = 0;
    virtual void DestroyTexture(TextureHandle texture) = 0;
    virtual void SetScissor(int x, int y, int w, int h) = 0;
    virtual void DrawTriangles(TextureHandle texture, const Vertex2D* vertices, int vertex_count,
                               const uint16_t* indices, int index_count) = 0;
};

class OverlayRenderer {
public:
    static OverlayRenderer& Get();

    // Liga o desenho pelas rotinas do jogo (retângulos e texto).
    void SetGameDraw(const game::GameDraw* draw) { game_draw_ = draw; }

    void SetBackend(IRenderBackend* backend) { backend_ = backend; }

    // Chamado no thread de render do jogo, dentro do hook de Render2dStuff.
    void RenderFrame();
    // Retorna true se o toque foi consumido pela UI do MTA.
    bool OnTouch(int action, int pointer, int x, int y);

private:
    struct Box {
        float x, y, w, h;
        bool Contains(float px, float py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };
    struct Layout {
        Box banner, menu_button, panel, close_button;
    };
    enum class Target : uint8_t { None, MenuButton, Panel, CloseButton };

    bool ComputeLayout(Layout* out) const;
    Target HitTest(const Layout& layout, float x, float y) const;
    void DrawStatusBanner(const Layout& layout);
    void DrawPanel(const Layout& layout);

    // O jogo aceita até 4 ponteiros de toque (ids 0..3).
    static constexpr int kMaxPointers = 4;

    const game::GameDraw* game_draw_ = nullptr;
    IRenderBackend* backend_ = nullptr;
    uint64_t frame_ = 0;

    // Toque e desenho rodam no GameThread; nada aqui precisa de trava.
    bool panel_open_ = false;
    bool pointer_down_[kMaxPointers] = {};
    Target pointer_target_[kMaxPointers] = {};
    int touch_log_budget_ = 6;
};

}  // namespace mtasa::gfx
