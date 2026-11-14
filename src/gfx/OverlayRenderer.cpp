#include "gfx/OverlayRenderer.h"

#include "mtasa/Log.h"

namespace mtasa::gfx {

OverlayRenderer& OverlayRenderer::Get() {
    static OverlayRenderer instance;
    return instance;
}

void OverlayRenderer::RenderFrame() {
    if (frame_++ == 0) LOGI("primeiro frame de overlay (backend=%p)", static_cast<void*>(backend_));
    if (!backend_) return;
    // TODO(fase 4): CEGUI::System::renderGUI() + fila de dxDraw* dos scripts.
}

bool OverlayRenderer::OnTouch(int action, int pointer, int x, int y) {
    // TODO(fase 4): injetar em CEGUI (injectMousePosition/ButtonDown/Up).
    (void)action; (void)pointer; (void)x; (void)y;
    return capture_input_;
}

}  // namespace mtasa::gfx
