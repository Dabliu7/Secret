#include "lua/LuaAllocator.h"

#include <cstdlib>

namespace mtasa::lua {

void* CappedAllocator::Alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* self = static_cast<CappedAllocator*>(ud);
    // Em Lua, quando ptr == nullptr, osize codifica o tipo do objeto, não um tamanho.
    const size_t old_size = ptr ? osize : 0;

    if (nsize == 0) {
        std::free(ptr);
        self->used_.fetch_sub(old_size, std::memory_order_relaxed);
        return nullptr;
    }

    const size_t used = self->used_.load(std::memory_order_relaxed);
    if (nsize > old_size && used - old_size + nsize > self->limit_) {
        return nullptr;  // Lua roda um GC de emergência e, se persistir, lança LUA_ERRMEM
    }

    void* result = std::realloc(ptr, nsize);
    if (!result) return nullptr;  // bloco antigo permanece válido

    const size_t now = self->used_.fetch_add(nsize - old_size, std::memory_order_relaxed) + nsize - old_size;
    if (now > self->peak_) self->peak_ = now;
    return result;
}

}  // namespace mtasa::lua
