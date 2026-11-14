#include "game/Symbols.h"

#include <cstring>
#include <dlfcn.h>

#include "mtasa/Log.h"

namespace mtasa::game {
namespace {

// Fallback para funções sem símbolo exportado: offset relativo à base, por build.
// Preenchida na Fase 0 a partir do dump (tools/dump_symbols.sh + Ghidra).
struct OffsetEntry {
    const char* name;
    uintptr_t offset;
};
const OffsetEntry kOffsets[] = {
    {nullptr, 0},  // sentinela; ex.: {"_Z13Render2dStuffv", 0x123456}
};

struct Wanted {
    const char* name;
    void* Symbols::*field;
};
// ATENÇÃO: nomes candidatos, a confirmar contra a .dynsym da build alvo (Fase 0).
const Wanted kWanted[] = {
    {"_ZN5CGame7ProcessEv", &Symbols::CGame_Process},
    {"_Z13Render2dStuffv", &Symbols::Render2dStuff},
    {"_Z14AND_TouchEventiiii", &Symbols::AND_TouchEvent},
};

}  // namespace

void* Resolve(const hook::ModuleInfo& module, const char* mangled_name) {
    if (module.handle) {
        if (void* p = dlsym(module.handle, mangled_name)) return p;
    }
    for (const auto& e : kOffsets) {
        if (e.name && std::strcmp(e.name, mangled_name) == 0)
            return reinterpret_cast<void*>(module.base + e.offset);
    }
    return nullptr;
}

int ResolveAll(const hook::ModuleInfo& module, Symbols* out) {
    int missing = 0;
    for (const auto& w : kWanted) {
        void* p = Resolve(module, w.name);
        out->*(w.field) = p;
        if (p) {
            LOGD("sym %-28s -> %p (+0x%lx)", w.name, p,
                 static_cast<unsigned long>(reinterpret_cast<uintptr_t>(p) - module.base));
        } else {
            LOGE("sym %s NÃO encontrado", w.name);
            ++missing;
        }
    }
    return missing;
}

}  // namespace mtasa::game
