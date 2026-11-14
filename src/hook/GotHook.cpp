#include <cstring>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>

#include "mtasa/Hook.h"
#include "mtasa/Log.h"

#if defined(__aarch64__)
#define MTASA_R_JUMP_SLOT R_AARCH64_JUMP_SLOT
#define MTASA_R_GLOB_DAT R_AARCH64_GLOB_DAT
#elif defined(__x86_64__)
#define MTASA_R_JUMP_SLOT R_X86_64_JUMP_SLOT
#define MTASA_R_GLOB_DAT R_X86_64_GLOB_DAT
#else
#error "arquitetura não suportada"
#endif

namespace mtasa::hook {
namespace {

struct Query {
    uintptr_t base;
    const ElfW(Dyn)* dynamic;
};

int FindDynamic(dl_phdr_info* info, size_t, void* user) {
    auto* q = static_cast<Query*>(user);
    if (static_cast<uintptr_t>(info->dlpi_addr) != q->base) return 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) {
            q->dynamic = reinterpret_cast<const ElfW(Dyn)*>(q->base + info->dlpi_phdr[i].p_vaddr);
            return 1;
        }
    }
    return 1;
}

bool WriteSlot(void** slot, void* value) {
    const uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    void* start = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(slot) & ~(page - 1));
    // A GOT pode estar em RELRO (somente leitura depois do link).
    if (mprotect(start, page, PROT_READ | PROT_WRITE) != 0) return false;
    *slot = value;
    return true;
}

}  // namespace

int HookGot(const ModuleInfo& module, const char* symbol, void* replacement, void** original) {
    Query q{module.base, nullptr};
    dl_iterate_phdr(FindDynamic, &q);
    if (!q.dynamic) return 0;

    // bionic guarda offsets em d_ptr; glibc guarda endereços já relocados.
    auto ptr = [&](ElfW(Addr) v) { return v < module.base ? module.base + v : static_cast<uintptr_t>(v); };

    const ElfW(Sym)* symtab = nullptr;
    const char* strtab = nullptr;
    const ElfW(Rela)* tables[2] = {nullptr, nullptr};  // .rela.plt, .rela.dyn
    size_t sizes[2] = {0, 0};
    for (const ElfW(Dyn)* d = q.dynamic; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SYMTAB: symtab = reinterpret_cast<const ElfW(Sym)*>(ptr(d->d_un.d_ptr)); break;
            case DT_STRTAB: strtab = reinterpret_cast<const char*>(ptr(d->d_un.d_ptr)); break;
            case DT_JMPREL: tables[0] = reinterpret_cast<const ElfW(Rela)*>(ptr(d->d_un.d_ptr)); break;
            case DT_PLTRELSZ: sizes[0] = d->d_un.d_val; break;
            case DT_RELA: tables[1] = reinterpret_cast<const ElfW(Rela)*>(ptr(d->d_un.d_ptr)); break;
            case DT_RELASZ: sizes[1] = d->d_un.d_val; break;
            default: break;
        }
    }
    if (!symtab || !strtab) return 0;

    int patched = 0;
    for (int t = 0; t < 2; ++t) {
        if (!tables[t]) continue;
        const size_t count = sizes[t] / sizeof(ElfW(Rela));
        for (size_t i = 0; i < count; ++i) {
            const ElfW(Rela)& rel = tables[t][i];
            const auto type = ELF64_R_TYPE(rel.r_info);
            if (type != MTASA_R_JUMP_SLOT && type != MTASA_R_GLOB_DAT) continue;
            const char* name = strtab + symtab[ELF64_R_SYM(rel.r_info)].st_name;
            if (std::strcmp(name, symbol) != 0) continue;
            void** slot = reinterpret_cast<void**>(module.base + rel.r_offset);
            if (*slot == replacement) continue;
            void* previous = *slot;
            if (!WriteSlot(slot, replacement)) {
                LOGE("HookGot: mprotect falhou para %s", symbol);
                continue;
            }
            if (original && patched == 0) *original = previous;
            ++patched;
        }
    }
    return patched;
}

}  // namespace mtasa::hook
