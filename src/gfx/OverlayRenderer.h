#pragma once
#include <cstdint>

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

    void SetBackend(IRenderBackend* backend) { backend_ = backend; }

    // Chamado no thread de render do jogo, dentro do hook de Render2dStuff.
    void RenderFrame();
    // Retorna true se o toque foi consumido pela UI do MTA.
    bool OnTouch(int action, int pointer, int x, int y);

private:
    IRenderBackend* backend_ = nullptr;
    uint64_t frame_ = 0;
    bool capture_input_ = false;
};

}  // namespace mtasa::gfx
