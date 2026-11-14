// Ponto de entrada da libmtasa.so.
//
// Carregamento: a activity do jogo chama System.loadLibrary("mtasa") logo após
// System.loadLibrary("GTASA") (patch de smali feito por tools/patch_apk.sh), ou
// a libGTASA.so recebe um DT_NEEDED para libmtasa.so. Nos dois casos rodamos
// antes do main loop nativo do jogo começar, então instalar hooks é seguro.
#include <atomic>
#include <chrono>
#include <thread>

#include "game/GameHooks.h"
#include "mtasa/Hook.h"
#include "mtasa/Log.h"
#include "net/NetTransport.h"

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace {

// Nome da biblioteca do motor: builds novas (casca Flutter) usam libGame.so,
// as clássicas usam libGTASA.so.
constexpr const char* kGameLibs[] = {"/libGame.so", "/libGTASA.so"};
std::atomic<bool> g_bootstrapped{false};

bool TryBootstrap() {
    mtasa::hook::ModuleInfo gtasa;
    for (const char* name : kGameLibs) {
        gtasa = mtasa::hook::FindModule(name);
        if (gtasa) break;
    }
    if (!gtasa) return false;
    if (g_bootstrapped.exchange(true)) return true;

    LOGI("motor do jogo em %p (%s)", reinterpret_cast<void*>(gtasa.base), gtasa.path);
    if (!mtasa::game::InstallHooks(gtasa)) {
        LOGE("bootstrap abortado: o jogo segue sem modificações");
    }
    return true;
}

// Fallback para quando somos carregados ANTES da libGTASA.so (ex.: DT_NEEDED).
void WaitForGameLibrary() {
    std::thread([] {
        using namespace std::chrono_literals;
        for (int i = 0; i < 3000; ++i) {  // ~30 s
            if (TryBootstrap()) return;
            std::this_thread::sleep_for(10ms);
        }
        LOGE("biblioteca do jogo não foi carregada; desistindo");
    }).detach();
}

void Entry() {
    LOGI("MTA:SA Android %s (hook=%s, net=%s)", MTASA_VERSION, mtasa::hook::BackendName(),
         mtasa::net::TransportName());
    LOGI("servidor de teste: %s", MTASA_TEST_SERVER[0] ? MTASA_TEST_SERVER : "(loopback)");
    if (!TryBootstrap()) WaitForGameLibrary();
}

}  // namespace

#ifdef __ANDROID__
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    Entry();
    return JNI_VERSION_1_6;
}
#else
// Build de host: só para compilar/lintar o código fora do NDK.
__attribute__((constructor)) static void HostEntry() { Entry(); }
#endif
