#include "game/GameHooks.h"

#include <atomic>

#include "game/Symbols.h"
#include "gfx/OverlayRenderer.h"
#include "mtasa/Log.h"

namespace mtasa::game {
namespace {

Symbols g_syms;
std::atomic<bool> g_installed{false};

// --- CGame::Process --------------------------------------------------------
// Thread principal do jogo. Aqui rodarão: pump de rede, pulso da VM Lua e
// aplicação de sync, sempre no mesmo thread que mexe nas entidades do jogo.
void (*orig_CGame_Process)() = nullptr;
void hook_CGame_Process() {
    static uint64_t frame = 0;
    if (frame++ == 0) LOGI("primeiro CGame::Process interceptado");
    // TODO(fase 3+): core::Pulse();
    orig_CGame_Process();
}

// --- Render2dStuff ---------------------------------------------------------
// Chamado depois do mundo 3D e antes do flip; o estado do RenderWare já está
// em modo 2D, então o overlay do MTA desenha por cima do HUD nativo.
void (*orig_Render2dStuff)() = nullptr;
void hook_Render2dStuff() {
    orig_Render2dStuff();
    gfx::OverlayRenderer::Get().RenderFrame();
}

// --- AND_TouchEvent --------------------------------------------------------
// Confirmado na build 2.11.311 pelo desmonte de GameNative_implOnTouch*:
// (ação, id do ponteiro, x, y) em pixels; ação 2 = toque/movimento, 1 = soltar.
void (*orig_AND_TouchEvent)(int, int, int, int) = nullptr;
void hook_AND_TouchEvent(int action, int pointer, int x, int y) {
    // Se a UI do MTA consumir o toque, o jogo não o recebe.
    if (gfx::OverlayRenderer::Get().OnTouch(action, pointer, x, y)) return;
    orig_AND_TouchEvent(action, pointer, x, y);
}

}  // namespace

bool InstallHooks(const hook::ModuleInfo& gtasa) {
    if (g_installed.exchange(true)) return true;

    if (ResolveAll(gtasa, &g_syms) != 0) {
        LOGE("símbolos obrigatórios ausentes: build da libGTASA.so não suportada");
        return false;
    }

    bool ok = true;
    ok &= hook::Install(g_syms.CGame_Process, hook_CGame_Process, &orig_CGame_Process);
    ok &= hook::Install(g_syms.Render2dStuff, hook_Render2dStuff, &orig_Render2dStuff);
    ok &= hook::Install(g_syms.AND_TouchEvent, hook_AND_TouchEvent, &orig_AND_TouchEvent);
    LOGI("hooks instalados: %s (backend %s)", ok ? "ok" : "FALHA", hook::BackendName());
    return ok;
}

}  // namespace mtasa::game
