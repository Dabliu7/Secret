#include "game/GameHooks.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <unistd.h>

#include "game/GameDraw.h"
#include "game/Symbols.h"
#include "gfx/OverlayRenderer.h"
#include "mtasa/Log.h"

namespace mtasa::game {
namespace {

Symbols g_syms;
GameDraw g_draw;
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

// --- ZIPFile::Find com `this` nulo -------------------------------------------
// Quando um asset pack não existe, ZIP_FileCreate devolve nullptr e o jogo chama
// ZIPFile::Find nesse ponteiro (queda vista em CAEMP3TrackLoader::GetDataStream).
// O retorno é {entrada, encontrado} em x0/x1; "não encontrado" já é um caminho
// tratado pelos chamadores.
struct ZipFindResult {
    uint64_t entry;
    uint64_t found;
};
ZipFindResult (*orig_ZIPFile_Find)(void*, void*) = nullptr;
ZipFindResult hook_ZIPFile_Find(void* self, void* name) {
    if (!self) return {0, 0};
    return orig_ZIPFile_Find(self, name);
}

// --- Streaming de áudio nulo -------------------------------------------------
// Em APKs reassinados a Play não entrega o pack `data_streams` (rádio, música,
// trilhas de cutscene e de missão). Os gerenciadores de trilha do jogo são
// máquinas de estado guiadas pelo código devolvido por GetTrackPlayTime, e há
// laços BLOQUEANTES esperando esses códigos (ex.: CAECutsceneTrackManager::
// PreloadCutsceneTrack espera -2). Em vez de desligar o serviço, emulamos um
// dispositivo que "toca" toda trilha instantaneamente:
//   PlayTrack          -> -2 (pronta)
//   StartTrackPlayback -> -6 (terminou)
//   StopTrack          -> -6
// Tudo via GOT: várias destas funções têm 2-3 instruções, pequenas demais para
// inline hook. Offsets de CAEStreamThread conferidos na build 2.11.311.
constexpr char kStreamsPackDir[] =
    "/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/mtasa_packs/data_streams";
constexpr int64_t kTrackReady = -2;
constexpr int64_t kTrackEnded = -6;
constexpr size_t kStreamThreadActiveTrackId = 0x50;
constexpr size_t kStreamThreadPlayingTrackId = 0x54;

std::atomic<int64_t> g_track_state{kTrackEnded};

void null_StreamThread_Service(void* /*self*/) {}

void null_StreamThread_PlayTrack(void* self, uint32_t track_id, int /*next_track_id*/,
                                 uint32_t /*start_ms*/, char /*type*/, bool, bool) {
    auto* fields = static_cast<uint8_t*>(self);
    *reinterpret_cast<uint32_t*>(fields + kStreamThreadActiveTrackId) = track_id;
    *reinterpret_cast<uint32_t*>(fields + kStreamThreadPlayingTrackId) = track_id;
    g_track_state.store(kTrackReady, std::memory_order_relaxed);
}

void null_StreamThread_StopTrack(void* /*self*/) {
    g_track_state.store(kTrackEnded, std::memory_order_relaxed);
}

int64_t null_StreamThread_GetTrackPlayTime(void* /*self*/) {
    return g_track_state.load(std::memory_order_relaxed);
}

void null_AudioHardware_StartTrackPlayback(void* /*self*/) {
    g_track_state.store(kTrackEnded, std::memory_order_relaxed);
}

// Sem trilhas não há por que ligar o rádio (e StopRadio tem laço bloqueante).
void null_RadioTrackManager_StartRadio4(void*, signed char, signed char, float, unsigned char) {}
void null_RadioTrackManager_StartRadio1(void*, void*) {}

void InstallAudioWorkarounds(const hook::ModuleInfo& gtasa) {
    const int find = hook::HookGot(
        gtasa, "_ZN7ZIPFile4FindENSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
        hook_ZIPFile_Find, &orig_ZIPFile_Find);
    LOGI("ZIPFile::Find protegido contra pack ausente: %d entrada(s)", find);

    if (access(kStreamsPackDir, F_OK) == 0) {
        LOGI("pack data_streams presente: streaming de áudio mantido");
        return;
    }

    struct Stub {
        const char* symbol;
        void* replacement;
    };
    const Stub stubs[] = {
        {"_ZN15CAEStreamThread7ServiceEv", reinterpret_cast<void*>(null_StreamThread_Service)},
        {"_ZN15CAEStreamThread9PlayTrackEjijcbb", reinterpret_cast<void*>(null_StreamThread_PlayTrack)},
        {"_ZN15CAEStreamThread9StopTrackEv", reinterpret_cast<void*>(null_StreamThread_StopTrack)},
        {"_ZN15CAEStreamThread16GetTrackPlayTimeEv",
         reinterpret_cast<void*>(null_StreamThread_GetTrackPlayTime)},
        {"_ZN16CAEAudioHardware18StartTrackPlaybackEv",
         reinterpret_cast<void*>(null_AudioHardware_StartTrackPlayback)},
        {"_ZN20CAERadioTrackManager10StartRadioEaafh",
         reinterpret_cast<void*>(null_RadioTrackManager_StartRadio4)},
        {"_ZN20CAERadioTrackManager10StartRadioEP21tVehicleAudioSettings",
         reinterpret_cast<void*>(null_RadioTrackManager_StartRadio1)},
    };
    int missing = 0;
    for (const auto& stub : stubs) {
        if (hook::HookGot(gtasa, stub.symbol, stub.replacement, nullptr) == 0) {
            LOGE("streaming nulo: %s sem entrada na GOT", stub.symbol);
            ++missing;
        }
    }
    LOGW("pack data_streams ausente: streaming de áudio nulo %s (sem rádio/música)",
         missing == 0 ? "ativo" : "INCOMPLETO; o jogo pode travar ou cair");
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
    InstallAudioWorkarounds(gtasa);
    if (ok && g_draw.Init(gtasa)) gfx::OverlayRenderer::Get().SetGameDraw(&g_draw);
    return ok;
}

}  // namespace mtasa::game
