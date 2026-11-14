#include "mtasa/Hook.h"

#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

#include "mtasa/Log.h"

#if defined(MTASA_HOOK_AND64)
#include "And64InlineHook.hpp"
#elif defined(MTASA_HOOK_DOBBY)
#include "dobby.h"
#endif

namespace mtasa::hook {

const char* BackendName() { return MTASA_HOOK_BACKEND_NAME; }

bool Install(void* target, void* replacement, void** original) {
    if (!target || !replacement) return false;
#if defined(MTASA_HOOK_AND64)
    void* trampoline = nullptr;
    A64HookFunction(target, replacement, &trampoline);
    if (!trampoline) return false;
    if (original) *original = trampoline;
    return true;
#elif defined(MTASA_HOOK_DOBBY)
    void* trampoline = nullptr;
    if (DobbyHook(target, replacement, &trampoline) != 0) return false;
    if (original) *original = trampoline;
    return true;
#else
    (void)original;
    LOGW("hook backend 'none': Install(%p) ignorado", target);
    return false;
#endif
}

bool Patch(void* address, const void* data, size_t size) {
    const long page = sysconf(_SC_PAGESIZE);  // 4 KB ou 16 KB: nunca assumir
    const auto start = reinterpret_cast<uintptr_t>(address) & ~static_cast<uintptr_t>(page - 1);
    const auto end = reinterpret_cast<uintptr_t>(address) + size;
    const size_t len = end - start;
    if (mprotect(reinterpret_cast<void*>(start), len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect(%p) falhou", address);
        return false;
    }
    std::memcpy(address, data, size);
    mprotect(reinterpret_cast<void*>(start), len, PROT_READ | PROT_EXEC);
    __builtin___clear_cache(static_cast<char*>(address), static_cast<char*>(address) + size);
    return true;
}

}  // namespace mtasa::hook
