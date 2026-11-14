#include "mtasa/Hook.h"

#include <cstring>
#include <dlfcn.h>
#include <link.h>

namespace mtasa::hook {
namespace {

struct Query {
    const char* suffix;
    ModuleInfo* out;
};

int OnModule(dl_phdr_info* info, size_t, void* user) {
    auto* q = static_cast<Query*>(user);
    const char* name = info->dlpi_name;
    if (!name) return 0;
    const size_t len = std::strlen(name);
    const size_t slen = std::strlen(q->suffix);
    if (len < slen || std::strcmp(name + len - slen, q->suffix) != 0) return 0;
    q->out->base = static_cast<uintptr_t>(info->dlpi_addr);
    std::strncpy(q->out->path, name, sizeof(q->out->path) - 1);
    return 1;  // encontrado, interrompe a iteração
}

}  // namespace

ModuleInfo FindModule(const char* name_suffix) {
    ModuleInfo info;
    Query q{name_suffix, &info};
    dl_iterate_phdr(OnModule, &q);
    if (info) {
        // Mesmo namespace do linker (lib do próprio app), então RTLD_NOLOAD resolve.
        info.handle = dlopen(info.path, RTLD_NOW | RTLD_NOLOAD);
        if (!info.handle) info.handle = dlopen(name_suffix, RTLD_NOW | RTLD_NOLOAD);
    }
    return info;
}

}  // namespace mtasa::hook
