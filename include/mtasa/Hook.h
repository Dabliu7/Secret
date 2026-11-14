#pragma once
#include <cstddef>
#include <cstdint>

namespace mtasa::hook {

// Informações de um módulo ELF já mapeado no processo.
struct ModuleInfo {
    uintptr_t base = 0;   // load bias (endereço = base + st_value / offset do IDA)
    void* handle = nullptr;  // handle de dlopen(RTLD_NOLOAD), para dlsym
    char path[512] = {};
    explicit operator bool() const { return base != 0; }
};

// Localiza um módulo carregado pelo sufixo do nome (ex.: "libGTASA.so").
ModuleInfo FindModule(const char* name_suffix);

// Instala um inline hook. `original` recebe o trampolim para a função original.
bool Install(void* target, void* replacement, void** original);

template <typename Fn>
bool Install(void* target, Fn* replacement, Fn** original) {
    return Install(target, reinterpret_cast<void*>(replacement), reinterpret_cast<void**>(original));
}

// Escreve bytes em memória de código (mprotect + flush de icache).
bool Patch(void* address, const void* data, size_t size);

const char* BackendName();

}  // namespace mtasa::hook
